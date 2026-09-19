#include "port/recomp_gx_fifo.h"
#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"
#include "aurora/lib/gx/fifo.hpp"

#include <dolphin/gx/GXFifo.h>
#include <dolphin/gx/GXFrameBuffer.h>
#include <dolphin/gx/GXGeometry.h>
#include <dolphin/gx/GXManage.h>

#include <cstring>
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
    // bridge, so today this is a safe no-op plus a log line.
    // GXWaitDrawDone (real address 0x80355CBC per generated_symbols.h)
    // has no Aurora implementation at all and isn't bridged yet - expect
    // it to show up as an unresolved-call log miss if/when reached.
    GXSetDrawDone();
    Log.info("GXSetDrawDone");
    return true;
}

bool host_call_gx_draw_done(CPUState *, u32)
{
    GXDrawDone();
    Log.info("GXDrawDone");
    return true;
}

bool host_call_gx_flush(CPUState *, u32)
{
    GXFlush();
    Log.info("GXFlush");
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

bool host_call_gx_copy_disp(CPUState *cpu, u32)
{
    // dest (r3) is a guest framebuffer address - not forwarded, since
    // Aurora's real GXCopyDisp is currently an empty stub regardless
    // (verified reading GXFrameBuffer.cpp) and never touches it. clear
    // (r4) is scalar and safe to pass through.
    const GXBool clear = static_cast<GXBool>(cpu->gpr[4]);
    GXCopyDisp(nullptr, clear);
    Log.info("GXCopyDisp(clear={})", static_cast<int>(clear));
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
