#include "port/recomp_gx_fifo.h"
#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"
#include "aurora/lib/gx/fifo.hpp"
#include "aurora/lib/gx/gx.hpp"

#include <dolphin/gx/GXFifo.h>
#include <dolphin/gx/GXAurora.h>
#include <dolphin/gx/GXCommandList.h>
#include <dolphin/gx/GXFrameBuffer.h>
#include <dolphin/gx/GXGeometry.h>
#include <dolphin/gx/GXManage.h>

#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sms::recomp::gx_fifo {
namespace {

aurora::Module Log("sms::recomp::gx_fifo");

// The real GX write-gather-pipe on hardware. Verified: dr_cpu's
// mem_write8/16/32/64 (extern/dolrecomp/src/cpu/cpu.c) already route any
// store to an address resolve_addr() can't map into guest RAM through
// CPUState::external_write - this range being outside GC_RAM_BASE/MEM1/
// MEM2 means every FIFO store the recompiled game executes reaches this
// handler as soon as it's registered, no other CPU-side change needed.
constexpr u32 kFifoBase = 0xCC008000u;
constexpr u32 kFifoEnd = 0xCC009000u;

void write(CPUState *, u32 /*addr*/, u64 value, u8 size)
{
    // The write-gather pipe ignores the specific address within the range
    // on real hardware (and aurora::gx::fifo's write_u8/u16/u32 API takes
    // no address either) - only the value and size matter.
    //
    // `value` here is dr_cpu's plain host-native integer (verified reading
    // mem_write32: it passes the same `value` it would otherwise
    // write_be32() into mapped RAM, not pre-byte-swapped) - exactly what
    // aurora::gx::fifo::write_u16/write_u32 expect, since they apply their
    // own bswap() internally before appending to the FIFO buffer
    // (extern/aurora/lib/gx/fifo.hpp).
    switch (size) {
        case 1:
            aurora::gx::fifo::write_u8(static_cast<u8>(value));
            break;
        case 2:
            aurora::gx::fifo::write_u16(static_cast<u16>(value));
            break;
        case 4:
            aurora::gx::fifo::write_u32(static_cast<u32>(value));
            break;
        case 8:
            // stfd/psq_st path - not yet exercised against a real run, so
            // the split direction (high word first) is a best guess from
            // big-endian byte order, not confirmed.
            aurora::gx::fifo::write_u32(static_cast<u32>(value >> 32));
            aurora::gx::fifo::write_u32(static_cast<u32>(value));
            break;
        default:
            Log.warn("FIFO write: unexpected size {}", size);
            break;
    }
}

} // namespace

void install()
{
    register_mmio_range({
        .base = kFifoBase,
        .end = kFifoEnd,
        .name = "GX FIFO",
        .read = nullptr, // write-only on real hardware; a read here is a
                          // signal something unexpected is happening, let
                          // the generic dispatcher's miss-log path catch it
        .write = &write,
    });
}

namespace {

// The host GXFifoObj* GXInit() really returns (extern/aurora/lib/dolphin/
// gx/GXManage.cpp - GXInit already calls aurora::gx::fifo::init() and
// wires both GXSetCPUFifo/GXSetGPFifo to it internally, verified reading
// that function). The guest never needs the real value - GXFifoObj is an
// opaque `u8 pad[128]` (extern/aurora/include/dolphin/gx/GXFifo.h) that
// Aurora's own GXSetCPUFifo/GXSetGPFifo never dereference (`CPUFifo =
// fifo;` is a plain pointer store) - so it's safe to hand the guest a
// fabricated sentinel instead and translate it back here.
GXFifoObj *g_realFifoObj = nullptr;

// The GameCube GXTexObj is a 32-byte big-endian object. Its image pointer is
// encoded in image3 as a physical address divided by 32. Aurora's
// GX_LOAD_AURORA_TEXOBJ carries a native host pointer instead, so the raw BP
// writes emitted by the guest's GXLoadTexObj alone cannot make its texture
// resolver see game RAM.
struct GuestTextureRevision {
    u64 signature = 0;
    u32 version = 1;
    u64 lastDiagnosticTimebase = 0;
};

std::unordered_map<u32, GuestTextureRevision> g_guestTextureRevisions;

// Bytes the base mip level occupies for a given GX texture format. Factored
// out because the *bounds check* needs it as much as the hash does: Aurora is
// handed a raw host pointer and will read width x height x bpp from it, so
// validating only the start address lets it run off the end of guest RAM.
u64 texture_source_bytes(u32 width, u32 height, u32 format)
{
    const u64 pixels = u64(width) * height;
    switch (format & 0x0Fu) {
    case 0: // I4
    case 8: // C4
        return (pixels + 1) / 2;
    case 2: // IA4
    case 9: // C8
        return pixels;
    case 3:  // IA8
    case 4:  // RGB565
    case 5:  // RGB5A3
    case 10: // C14X2
        return pixels * 2;
    case 6: // RGBA8
        return pixels * 4;
    default: // I8 and unknown extended formats: one byte is the safest base.
        return pixels;
    }
}

u64 sampled_texture_content_signature(const u8 *data, u32 availableBytes, u32 width, u32 height, u32 format)
{
    // Aurora caches decoded textures by (object ID, data revision), while a
    // GameCube GXTexObj has no matching revision field. Sample the base level
    // so writes into a reused movie/render buffer advance that revision without
    // needlessly re-uploading ordinary immutable textures.
    const u32 bytes = static_cast<u32>(std::min<u64>(texture_source_bytes(width, height, format), availableBytes));
    if (bytes == 0) {
        return 0;
    }

    u64 hash = 1469598103934665603ull;
    constexpr u32 kSampleCount = 64;
    for (u32 i = 0; i < kSampleCount; ++i) {
        const u32 index = static_cast<u32>((u64(i) * (bytes - 1)) / (kSampleCount - 1));
        hash ^= data[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

bool emit_guest_texture_metadata(CPUState *cpu, u32 textureMap)
{
    constexpr u32 kGuestTexObjSize = 0x20;
    constexpr u32 kGuestImage0Offset = 0x08;
    constexpr u32 kGuestImage3Offset = 0x0C;
    constexpr u32 kGuestFormatOffset = 0x14;
    constexpr u32 kGuestTlutOffset = 0x18;
    constexpr u32 kGuestFlagsOffset = 0x1F;
    constexpr u32 kTextureMapCount = 8;

    const u32 objectAddress = cpu->gpr[3];
    const u32 objectOffset = objectAddress & 0x03FFFFFFu;
    if (textureMap >= kTextureMapCount || objectOffset > cpu->ram_size || kGuestTexObjSize > cpu->ram_size - objectOffset) {
        return false;
    }

    const u32 image0 = mem_read32(cpu, objectAddress + kGuestImage0Offset);
    const u32 image3 = mem_read32(cpu, objectAddress + kGuestImage3Offset);
    const u32 format = mem_read32(cpu, objectAddress + kGuestFormatOffset);
    const u32 tlut = mem_read32(cpu, objectAddress + kGuestTlutOffset);
    const u8 flags = mem_read8(cpu, objectAddress + kGuestFlagsOffset);

    const u32 width = (image0 & 0x3FFu) + 1;
    const u32 height = ((image0 >> 10) & 0x3FFu) + 1;
    const u32 imageAddress = (image3 & 0x001FFFFFu) << 5;
    const u32 imageOffset = imageAddress & 0x03FFFFFFu;
    // Validate the whole extent, not just the start. Aurora receives a bare
    // host pointer and decodes width x height x bpp bytes from it, so an
    // address that is merely *inside* RAM is not enough - a texture starting
    // near the top would be read past the end of the guest's memory.
    const u64 sourceBytes = texture_source_bytes(width, height, format);
    if (imageOffset >= cpu->ram_size || sourceBytes > cpu->ram_size - imageOffset) {
        // Capped by count as well as deduplicated: imageAddress comes from a
        // 21-bit field shifted left by 5, so up to two million distinct keys
        // are reachable. Deduplication alone bounds neither the log nor the
        // set - the exact mistake already corrected in recomp_host.cpp.
        static std::unordered_set<u32> warned;
        if (warned.size() < 16 && warned.insert(imageAddress).second) {
            Log.warn("GXLoadTexObj image {:#010x} + {:#x} bytes runs past guest RAM - Aurora texture metadata not emitted",
                imageAddress, sourceBytes);
        }
        return false;
    }

    // A stable ID lets Aurora retain decoded static textures. Bump the
    // revision whenever the guest object describes different source data.
    const u64 contentSignature = sampled_texture_content_signature(
        cpu->ram + imageOffset, cpu->ram_size - imageOffset, width, height, format);
    const u64 signature = (u64(image0) << 32) ^ u64(image3) ^ (u64(format) << 17) ^ (u64(tlut) << 1) ^ flags ^ contentSignature;
    auto [revisionIt, inserted] = g_guestTextureRevisions.try_emplace(objectAddress, GuestTextureRevision { signature, 1, 0 });
    if (!inserted && revisionIt->second.signature != signature) {
        revisionIt->second.signature = signature;
        ++revisionIt->second.version;
        if (revisionIt->second.version == 0) {
            revisionIt->second.version = 1;
        }
    }
    if (inserted && g_guestTextureRevisions.size() <= 8) {
        Log.info("GXLoadTexObj bridge: map={} object={:#010x} image={:#010x} {}x{} format={:#x}",
            textureMap, objectAddress, imageAddress, width, height, format);
    }
    // THPPlayer converts its decoded Y/U/V planes into exactly these I8
    // dimensions. One bounded diagnostic per second tells us whether their
    // bytes actually evolve before they reach Aurora's TEV combiner.
    const bool thpPlane = format == 1u
        && ((width == 640u && height == 448u) || (width == 320u && height == 224u));
    if (thpPlane && cpu->timebase - revisionIt->second.lastDiagnosticTimebase >= 40500000ull) {
        // Scan the WHOLE plane, not a sample of it. The 64-point signature
        // above is a change detector, not a content detector: 64 bytes out of
        // a 286,720-byte plane is 0.02%, so "all sampled bytes are zero" does
        // not distinguish a decoder that wrote nothing from a sample that
        // happened to miss everything. This is the measurement that does.
        //
        // One full pass per plane per second is nothing next to the decode
        // that produced it, and the caller is already throttled to that rate.
        const u8 *plane = cpu->ram + imageOffset;
        u64 nonZero = 0;
        u64 sum = 0;
        u8 minByte = 0xFFu;
        u8 maxByte = 0;
        for (u64 i = 0; i < sourceBytes; ++i) {
            const u8 value = plane[i];
            if (value != 0) {
                ++nonZero;
            }
            sum += value;
            minByte = value < minByte ? value : minByte;
            maxByte = value > maxByte ? value : maxByte;
        }
        // The mean is what distinguishes "the renderer is broken" from "the
        // movie opens on a dark frame". A non-zero count alone cannot: a plane
        // that is 90% non-zero can still be almost entirely near-black.
        const double mean = sourceBytes == 0 ? 0.0 : double(sum) / double(sourceBytes);
        Log.info("THP plane: map={} image={:#010x} {}x{} bytes={:#x} nonZero={:.1f}% mean={:.1f} min={} max={} revision={}",
            textureMap, imageAddress, width, height, sourceBytes,
            sourceBytes == 0 ? 0.0 : (100.0 * double(nonZero) / double(sourceBytes)), mean, minByte, maxByte,
            revisionIt->second.version);
        revisionIt->second.lastDiagnosticTimebase = cpu->timebase;
    }

    // Emit Aurora's documented software-FIFO metadata first, then return
    // false so DolRecomp runs the original GXLoadTexObj. That retains the
    // guest GX shadow-state updates and the normal BP register sequence.
    aurora::gx::fifo::write_u8(GX_LOAD_AURORA);
    aurora::gx::fifo::write_u16(GX_LOAD_AURORA_TEXOBJ);
    aurora::gx::fifo::write_u8(static_cast<u8>(textureMap));
    aurora::gx::fifo::write_u64(reinterpret_cast<u64>(cpu->ram + imageOffset));
    aurora::gx::fifo::write_u32(width);
    aurora::gx::fifo::write_u32(height);
    aurora::gx::fifo::write_u32(format);
    aurora::gx::fifo::write_u32(tlut);
    aurora::gx::fifo::write_u8((flags & 1u) != 0);
    aurora::gx::fifo::write_u32(objectAddress);
    aurora::gx::fifo::write_u32(revisionIt->second.version);
    return true;
}

bool host_call_gx_init(CPUState *, u32)
{
    // Aurora's GXInit is a faithful reimplementation - it runs
    // aurora::gx::fifo::init() and then replays the whole default register
    // block (GXSetTexCoordGen x8, GXSetNumTexGens(1), GXSetTevOrder x16,
    // GXSetNumTevStages(1), GXSetTevOp(GX_REPLACE), ...) through that fifo,
    // which is what brings g_gxState up out of its struct defaults. So it
    // still has to run, and it has to run first, to prepare the host side.
    static bool auroraInitDone = false;
    if (!auroraInitDone) {
        auroraInitDone = true;
        g_realFifoObj = GXInit(nullptr, 0);
        Log.info("GXInit -> aurora fifo initialized, deferring to the guest's own GXInit");
    }

    // ...but replacing the guest's GXInit outright was wrong, and measurably
    // so. The guest's GX library keeps its own shadow state (`gxData`,
    // src/dolphin/gx/GXInit.c) whose *register address* bytes are installed
    // by that very function - SET_REG_FIELD(..., 8, 24, 0xC0 + i * 2) and
    // friends. Skipping it leaves gxData zeroed in BSS, so the first time the
    // game flushes its dirty state it emits genMode (BP register 0x00) as a
    // plain zero: ntex=0, nchan=0, ntevstages=0+1=1. That silently clobbered
    // Aurora's numTexGens back to 0 while leaving the TEV orders intact, and
    // the first textured draw then died in the shader builder with
    // "unhandled tcg src 21" (GX_MAX_TEXGENSRC, TcgConfig's never-configured
    // sentinel) on a state dump that matched a zero genMode exactly.
    //
    // Returning false hands control back to dolrecomp_call, which falls
    // through to dolrecomp_call_original and runs the real GXInit. It
    // initializes gxData properly and re-emits the same defaults through the
    // write-gather pipe, so Aurora's state is rewritten with identical values
    // rather than corrupted ones. It also returns the guest's own &FifoObj in
    // r3, which is a genuine guest address - no fabricated handle needed.
    return false;
}

bool host_call_gx_set_cpu_fifo(CPUState *, u32)
{
    // The guest-supplied handle (r3) is intentionally ignored - see
    // g_realFifoObj's comment. It is now the guest's real &FifoObj, since
    // the guest runs its own GXInit, but Aurora's GXSetCPUFifo never
    // dereferences what it is handed (`CPUFifo = fifo;` is a plain pointer
    // store), so forwarding the host object instead stays safe.
    GXSetCPUFifo(g_realFifoObj);
    Log.info("GXSetCPUFifo (real fifo object)");
    return true;
}

bool host_call_gx_set_gp_fifo(CPUState *, u32)
{
    GXSetGPFifo(g_realFifoObj);
    Log.info("GXSetGPFifo (real fifo object)");
    return true;
}

bool host_call_gx_set_draw_done(CPUState *, u32)
{
    // This SDK revision's actual symbol is GXSetDrawDone (no callback
    // argument - confirmed present in generated/generated_symbols.h,
    // unlike GXSetDrawDoneCallback, which this dump doesn't have at all).
    // Aurora's real GXSetDrawDone() just synchronously invokes whatever
    // callback GXSetDrawDoneCallback registered (extern/aurora/lib/
    // dolphin/gx/GXManage.cpp) - none has been registered through this
    // bridge, so today this is a safe no-op.
    // GXWaitDrawDone (real address 0x80355CBC per generated_symbols.h)
    // has no Aurora implementation at all and isn't bridged yet - expect
    // it to show up as an unresolved-call log miss if/when reached.
    GXSetDrawDone();
    return true;
}

bool host_call_gx_draw_done(CPUState *, u32)
{
    GXDrawDone();
    return true;
}

bool host_call_gx_flush(CPUState *, u32)
{
    GXFlush();
    return true;
}

// GXSetArray is the one GX call that cannot work by simply forwarding FIFO
// bytes, because what it puts in the FIFO is a *pointer*.
//
// On hardware the CP array-base registers hold a guest physical address and
// the GP walks guest RAM itself. Aurora has no guest RAM to walk: it rejects
// those registers outright ("CP_REG_ARRAYBASE_ID is not supported on Aurora.
// Use GX_LOAD_AURORA_ARRAYBASE instead.", lib/gx/command_processor.cpp) and
// offers its own command carrying a 64-bit *host* pointer instead.
//
// So every indexed vertex attribute the game set up was dropped, and with
// it every piece of geometry drawn from an array - which is essentially all
// of them. Measured: the render loop ran and presented 6613 frames in 6791,
// and every one of them was a flat clear colour.
//
// Translating guest pointer to host pointer is exactly this bridge's job.
bool host_call_gx_set_array(CPUState *cpu, u32)
{
    const auto attr = static_cast<GXAttr>(cpu->gpr[3]);
    const u32 guestPointer = cpu->gpr[4];
    const auto stride = static_cast<u8>(cpu->gpr[5]);

    // Cached (0x8...), uncached (0xC...) and physical all name the same bytes;
    // masking off the region nibble is what the real DMA engines do too, and
    // matches how the ARAM and DI transfers in recomp_exi.cpp resolve theirs.
    const u32 offset = guestPointer & 0x03FFFFFFu;
    if (offset >= cpu->ram_size) {
        static std::unordered_set<u32> warned;
        if (warned.insert(guestPointer).second) {
            Log.warn("GXSetArray(attr={}) base {:#010x} is outside guest RAM - array not forwarded",
                static_cast<int>(attr), guestPointer);
        }
        return true;
    }

    // The SDK's GXSetArray has no size argument; the hardware has no size
    // register either, only a stride. Aurora wants one for its own upload
    // bookkeeping, so it gets the only bound that is actually known: the array
    // cannot run past the end of guest RAM.
    const u32 size = cpu->ram_size - offset;

    // GameCube vertex data is big-endian, hence le = false.
    GXSetArray(attr, cpu->ram + offset, size, stride, false);
    return true;
}

bool host_call_gx_load_tex_obj(CPUState *cpu, u32)
{
    emit_guest_texture_metadata(cpu, cpu->gpr[4]);
    // Keep the original GameCube SDK body: it updates guest gxData as well
    // as emitting BP state that Aurora's FIFO decoder still consumes.
    return false;
}

bool host_call_gx_load_tex_obj_preloaded(CPUState *cpu, u32)
{
    // GXLoadTexObjPreLoaded(obj, region, id) puts the texture-map ID in r5,
    // unlike GXLoadTexObj(obj, id), which puts it in r4. Some game paths
    // call this lower-level API directly, so bridge both entry points.
    emit_guest_texture_metadata(cpu, cpu->gpr[5]);
    return false;
}

bool host_call_gx_invalidate_tex_all(CPUState *, u32)
{
    // The guest routine only writes GameCube texture-cache invalidation BP
    // registers. Aurora has a host-side decoded-texture cache as well, which
    // those raw registers cannot invalidate. This is crucial for THP video
    // frames: the same GXTexObj points at pixels overwritten every frame.
    aurora::gx::clear_static_texture_cache();
    return false;
}

bool host_call_gx_copy_disp(CPUState *cpu, u32)
{
    // dest (r3) is a guest framebuffer address - not forwarded, since
    // Aurora's real GXCopyDisp is currently an empty stub regardless
    // (verified reading GXFrameBuffer.cpp) and never touches it. clear
    // (r4) is scalar and safe to pass through.
    const GXBool clear = static_cast<GXBool>(cpu->gpr[4]);
    GXCopyDisp(nullptr, clear);
    return true;
}

} // namespace

void register_known_gx_calls(const NamedAddress *addresses, size_t count)
{
    std::vector<HostCallEntry> entries;
    for (size_t i = 0; i < count; ++i) {
        const char *name = addresses[i].name;
        HostCallFn fn = nullptr;
        if (std::strcmp(name, "GXInit") == 0) {
            fn = &host_call_gx_init;
        } else if (std::strcmp(name, "GXSetCPUFifo") == 0) {
            fn = &host_call_gx_set_cpu_fifo;
        } else if (std::strcmp(name, "GXSetGPFifo") == 0) {
            fn = &host_call_gx_set_gp_fifo;
        } else if (std::strcmp(name, "GXSetDrawDone") == 0) {
            fn = &host_call_gx_set_draw_done;
        } else if (std::strcmp(name, "GXDrawDone") == 0) {
            fn = &host_call_gx_draw_done;
        } else if (std::strcmp(name, "GXFlush") == 0) {
            fn = &host_call_gx_flush;
        } else if (std::strcmp(name, "GXCopyDisp") == 0) {
            fn = &host_call_gx_copy_disp;
        } else if (std::strcmp(name, "GXSetArray") == 0) {
            fn = &host_call_gx_set_array;
        } else if (std::strcmp(name, "GXLoadTexObj") == 0) {
            fn = &host_call_gx_load_tex_obj;
        } else if (std::strcmp(name, "GXLoadTexObjPreLoaded") == 0) {
            fn = &host_call_gx_load_tex_obj_preloaded;
        } else if (std::strcmp(name, "GXInvalidateTexAll") == 0) {
            fn = &host_call_gx_invalidate_tex_all;
        }
        if (fn) {
            entries.push_back({ addresses[i].address, name, fn });
        }
        // Everything else in the GX family relies purely on install()'s
        // external_write forwarding above - only functions that configure
        // Aurora's fifo module or callback wiring, rather than writing
        // FIFO bytes themselves, need a bridge here at all.
    }
    if (!entries.empty()) {
        register_host_calls(entries.data(), entries.size());
    }
}

} // namespace sms::recomp::gx_fifo
