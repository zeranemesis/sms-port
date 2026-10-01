#include "port/recomp_gx_copy.h"

#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"
#include "aurora/lib/gfx/common.hpp"
#include "aurora/lib/gx/gx.hpp"

#include <dolphin/gx/GXFrameBuffer.h>

#include <cstring>
#include <vector>

namespace sms::recomp::gx_copy {
namespace {

aurora::Module Log("sms::recomp::gx_copy");

// 40.5MHz timebase, so one second of guest time - the same figure
// recomp_gx_fifo.cpp's THP-plane diagnostic throttles itself with.
constexpr u64 kReportIntervalTimebase = 40500000ull;

// A gxData read that is not in guest RAM is not a value to decode, it is a
// wrong assumption about where `gx` points. Bounds-checked rather than clamped,
// so a disc whose link order differs says so instead of decoding garbage.
bool read_guest_u32(CPUState *cpu, u32 addr, u32 *out)
{
    const u32 offset = addr & 0x03FFFFFFu;
    if (offset > cpu->ram_size || sizeof(u32) > cpu->ram_size - offset) {
        return false;
    }
    *out = mem_read32(cpu, addr);
    return true;
}

bool read_guest_f32(CPUState *cpu, u32 addr, float *out)
{
    u32 bits = 0;
    if (!read_guest_u32(cpu, addr, &bits)) {
        return false;
    }
    // Single-precision arguments arrive in a GPR as raw IEEE-754 bits under the
    // PowerPC EABI - it is the callee's prologue that moves them into the FPR
    // file. Only the viewport in the per-second report reads these, so if the
    // convention turned out to be otherwise the number in one log line would
    // be wrong and nothing else would be.
    static_assert(sizeof(bits) == sizeof(*out), "f32 is not 4 bytes here");
    std::memcpy(out, &bits, sizeof(bits));
    return true;
}

bool read_guest_shadow(CPUState *cpu, GuestShadow *shadow)
{
    u32 gxAddress = 0;
    if (!read_guest_u32(cpu, shadow::kGuestGxPointer, &gxAddress)) {
        return false;
    }
    const u32 gxOffset = gxAddress & 0x03FFFFFFu;
    if (gxOffset > cpu->ram_size || shadow::kGuestGxDataSize > cpu->ram_size - gxOffset) {
        return false;
    }
    // Re-derive the region base from the pointer rather than assuming 0x8xxxxxxx:
    // the guest may hand out an uncached alias, and the 26-bit offset is the
    // only part that identifies the bytes.
    const u32 base = gxAddress & ~0x03FFFFFFu;

    return read_guest_u32(cpu, base + shadow::kCmode0, &shadow->cmode0)
        && read_guest_u32(cpu, base + shadow::kCmode1, &shadow->cmode1)
        && read_guest_u32(cpu, base + shadow::kZmode, &shadow->zmode)
        && read_guest_u32(cpu, base + shadow::kPeCtrl, &shadow->peCtrl)
        && read_guest_u32(cpu, base + shadow::kCpTexSrc, &shadow->cpTexSrc)
        && read_guest_u32(cpu, base + shadow::kCpTexSize, &shadow->cpTexSize)
        && read_guest_u32(cpu, base + shadow::kCpTexStride, &shadow->cpTexStride)
        && read_guest_u32(cpu, base + shadow::kCpTex, &shadow->cpTex)
        && read_guest_u32(cpu, base + shadow::kCpTexZ, &shadow->cpTexZ)
        && read_guest_f32(cpu, base + shadow::kVpLeft, &shadow->vpLeft)
        && read_guest_f32(cpu, base + shadow::kVpTop, &shadow->vpTop)
        && read_guest_f32(cpu, base + shadow::kVpWd, &shadow->vpWd)
        && read_guest_f32(cpu, base + shadow::kVpHt, &shadow->vpHt);
}

// Bytes a decoded texture of this shape occupies, for the same reason
// recomp_gx_fifo.cpp validates the whole extent: Aurora receives a bare host
// pointer and decodes width x height x bpp from it, so an address merely
// inside guest RAM is not enough.
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

// Per-second copy report. This is the measurement docs/port_todo.md section 1.0
// says is missing: whether the game asks for a render-to-texture at all, with
// what source rectangle and destination size, and under what viewport. The
// instrumentation that last produced `onscreen=9627 offscreen=0 ... viewport=(4,4
// 1024x1024)` is not in the tree, and without a number like this a bridge to
// GXCopyTex is a guess with no way to tell afterwards whether it helped.
struct CopyReport {
    u64 setSrc = 0;
    u64 setDst = 0;
    u64 copyTex = 0;
    u64 realCopies = 0;
    u64 lastReportTimebase = 0;
    bool reported = false;

    // The last copy seen, so the report names a concrete pass rather than only
    // counting them. viewport is tracked here and not in CopyPlan because
    // CopyPlan is the pure decode result that run_gx_copy_self_test() checks.
    CopyPlan lastPlan;
    float vpLeft = 0.0f;
    float vpTop = 0.0f;
    float vpWd = 0.0f;
    float vpHt = 0.0f;
    u32 dstWidth = 0;
    u32 dstHeight = 0;
    u32 dstFormat = 0;
    bool clear = false;
    bool copyUnresolved = false;
};

CopyReport &report()
{
    static CopyReport instance;
    return instance;
}

// Scissor, decoded exactly as GXSetScissor encodes it (GXTransform.c:314-322)
// and as Aurora decodes it back (command_processor.cpp:630-644), so the number
// in the log is the one the hardware would have been given.
void read_scissor(aurora::gfx::ClipRect *out)
{
    const u32 scis0 = aurora::gx::g_gxState.bpRegCache[0x20];
    const u32 scis1 = aurora::gx::g_gxState.bpRegCache[0x21];
    const s32 tp = static_cast<s32>(scis0 & 0x7FFu) - 342;
    const s32 lf = static_cast<s32>((scis0 >> 12) & 0x7FFu) - 342;
    const s32 bm = static_cast<s32>(scis1 & 0x7FFu) - 342;
    const s32 rt = static_cast<s32>((scis1 >> 12) & 0x7FFu) - 342;
    out->x = lf;
    out->y = tp;
    out->width = rt > lf ? rt - lf + 1 : 0;
    out->height = bm > tp ? bm - tp + 1 : 0;
}

void maybe_report(CPUState *cpu)
{
    CopyReport &r = report();
    if (!r.reported) {
        r.lastReportTimebase = cpu->timebase;
        r.reported = true;
        return;
    }
    if (cpu->timebase < r.lastReportTimebase + kReportIntervalTimebase) {
        return;
    }
    r.lastReportTimebase = cpu->timebase;

    if (r.setSrc == 0 && r.setDst == 0 && r.copyTex == 0) {
        // Nothing at all. Said explicitly, because silence here is
        // indistinguishable from a bridge that was never reached - which is
        // the whole question this report exists to answer.
        Log.info("copy 1s: no GXSetTexCopySrc / GXSetTexCopyDst / GXCopyTex call - "
                 "the game is not asking for render-to-texture");
    } else {
        aurora::gfx::ClipRect scissor {};
        read_scissor(&scissor);
        Log.info("copy 1s: setSrc={} setDst={} copyTex={} real={} last=({}x{} at {},{} -> {}x{} fmt={:#x} "
                 "clear={}{}) viewport=({},{} {}x{}) scissor=({},{} {}x{})",
            r.setSrc, r.setDst, r.copyTex, r.realCopies,
            r.lastPlan.srcWidth, r.lastPlan.srcHeight, r.lastPlan.srcLeft, r.lastPlan.srcTop,
            r.dstWidth, r.dstHeight, r.dstFormat, r.clear,
            r.copyUnresolved ? " UNRESOLVED" : "",
            static_cast<int>(r.vpLeft), static_cast<int>(r.vpTop),
            static_cast<int>(r.vpWd), static_cast<int>(r.vpHt),
            scissor.x, scissor.y, scissor.width, scissor.height);
    }
    r.setSrc = 0;
    r.setDst = 0;
    r.copyTex = 0;
    r.realCopies = 0;
    r.copyUnresolved = false;
}

// The observers below all return false, which hands the call back to the
// recompiled guest body. That is the point rather than a limitation: the guest
// body is what maintains gxData and pushes the BP registers, and every one of
// them is still needed for state this file does not model. Only GXCopyTex does
// work of its own, and it still returns false for the same reason - see the
// note at the end of host_call_gx_copy_tex.
bool host_call_gx_set_tex_copy_src(CPUState *cpu, u32)
{
    CopyReport &r = report();
    ++r.setSrc;
    GuestShadow shadow;
    if (read_guest_shadow(cpu, &shadow)) {
        r.lastPlan = decode_copy_plan(shadow);
    }
    maybe_report(cpu);
    return false;
}

bool host_call_gx_set_tex_copy_dst(CPUState *cpu, u32)
{
    report().setDst++;
    maybe_report(cpu);
    return false;
}

bool host_call_gx_set_viewport(CPUState *cpu, u32)
{
    CopyReport &r = report();
    GuestShadow shadow;
    if (read_guest_shadow(cpu, &shadow)) {
        r.vpLeft = shadow.vpLeft;
        r.vpTop = shadow.vpTop;
        r.vpWd = shadow.vpWd;
        r.vpHt = shadow.vpHt;
    }
    return false;
}

bool host_call_gx_set_scissor(CPUState *, u32)
{
    return false;
}

bool host_call_gx_copy_tex(CPUState *cpu, u32)
{
    CopyReport &r = report();
    ++r.copyTex;

    const u32 objectAddress = cpu->gpr[3];
    const bool clear = cpu->gpr[4] != 0;

    GuestShadow shadow;
    const bool haveShadow = read_guest_shadow(cpu, &shadow);
    r.lastPlan = haveShadow ? decode_copy_plan(shadow) : CopyPlan {};
    r.clear = clear;
    r.copyUnresolved = false;
    r.dstWidth = 0;
    r.dstHeight = 0;
    r.dstFormat = 0;

    if (is_real_copy(r.lastPlan)) {
        ++r.realCopies;
    }
    if (!haveShadow) {
        r.copyUnresolved = true;
        maybe_report(cpu);
        return false;
    }

    // The destination is a guest GXTexObj pointer, and its dimensions and format
    // come out of that object rather than out of gxData: GXSetTexCopyDst keeps
    // only a tile count in cpTexStride, which is not the size, so the size is
    // only recoverable from the object the call was handed.
    const u32 objectOffset = objectAddress & 0x03FFFFFFu;
    if (objectOffset > cpu->ram_size || shadow::kGuestTexObjSize > cpu->ram_size - objectOffset) {
        r.copyUnresolved = true;
        maybe_report(cpu);
        return false;
    }

    const u32 image0 = mem_read32(cpu, objectAddress + shadow::kGuestTexObjImage0);
    const u32 image3 = mem_read32(cpu, objectAddress + shadow::kGuestTexObjImage3);
    const u32 dstFormat = mem_read32(cpu, objectAddress + shadow::kGuestTexObjFormat);
    const u32 dstWidth = (image0 & 0x3FFu) + 1;
    const u32 dstHeight = ((image0 >> 10) & 0x3FFu) + 1;
    const u32 imageAddress = (image3 & 0x001FFFFFu) << 5;
    const u32 imageOffset = imageAddress & 0x03FFFFFFu;
    const u64 sourceBytes = texture_source_bytes(dstWidth, dstHeight, dstFormat);

    r.dstWidth = dstWidth;
    r.dstHeight = dstHeight;
    r.dstFormat = dstFormat;

    if (imageOffset >= cpu->ram_size || sourceBytes > cpu->ram_size - imageOffset) {
        r.copyUnresolved = true;
        maybe_report(cpu);
        return false;
    }

    // Aurora keys g_gxState.copyTextures by the pointer it is handed, and
    // lib/gx/gx.cpp looks that map up later with GXTexObj_::data - which
    // recomp_gx_fifo.cpp's emit_guest_texture_metadata sets to exactly
    // cpu->ram + imageOffset for this same texture. Handing GXCopyTex the guest
    // GXTexObj address instead would resolve a texture nothing ever looks up,
    // and the copy would look bridged while staying invisible.
    aurora::gx::GXState &state = aurora::gx::g_gxState;
    state.texCopySrc = { static_cast<s32>(r.lastPlan.srcLeft), static_cast<s32>(r.lastPlan.srcTop),
        static_cast<s32>(r.lastPlan.srcWidth), static_cast<s32>(r.lastPlan.srcHeight) };
    state.texCopyDstWidth = static_cast<u16>(dstWidth);
    state.texCopyDstHeight = static_cast<u16>(dstHeight);
    state.texCopyFmt = static_cast<GXTexFmt>(dstFormat);
    state.colorUpdate = r.lastPlan.colorUpdate;
    state.alphaUpdate = r.lastPlan.alphaUpdate;
    state.depthUpdate = r.lastPlan.depthUpdate;

    // GXCopyTex drains the FIFO itself, so everything the guest queued before
    // this call is in the EFB by the time the copy resolves.
    ::GXCopyTex(cpu->ram + imageOffset, clear ? GX_TRUE : GX_FALSE);

    maybe_report(cpu);
    // false, so the recompiled guest body still runs: it is what clears
    // gx->bpSent and keeps the guest's own shadow state in step with what the
    // hardware would have been told. Same reasoning as
    // recomp_gx_fifo.cpp's host_call_gx_load_tex_obj.
    return false;
}

} // namespace

void register_known_gx_copy_calls(const NamedAddress *addresses, size_t count)
{
    std::vector<HostCallEntry> entries;
    entries.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const char *name = addresses[i].name;
        HostCallFn fn = nullptr;
        if (std::strcmp(name, "GXSetTexCopySrc") == 0) {
            fn = &host_call_gx_set_tex_copy_src;
        } else if (std::strcmp(name, "GXSetTexCopyDst") == 0) {
            fn = &host_call_gx_set_tex_copy_dst;
        } else if (std::strcmp(name, "GXCopyTex") == 0) {
            fn = &host_call_gx_copy_tex;
        } else if (std::strcmp(name, "GXSetViewport") == 0) {
            fn = &host_call_gx_set_viewport;
        } else if (std::strcmp(name, "GXSetScissor") == 0) {
            fn = &host_call_gx_set_scissor;
        }
        if (fn != nullptr) {
            entries.push_back({ addresses[i].address, name, fn });
        }
    }
    if (!entries.empty()) {
        register_host_calls(entries.data(), entries.size());
    }
}

bool run_gx_copy_self_test()
{
    // A shadow image shaped exactly as the SDK's own setters would have left
    // it, so the expected numbers come from GXFrameBuf.c's SET_REG_FIELD
    // positions rather than from this decoder's own behaviour.
    GuestShadow shadow;
    // GXSetTexCopySrc(64, 32, 512, 256)
    shadow.cpTexSrc = (64u & 0x3FFu) | ((32u & 0x3FFu) << 10) | (0x49u << 24);
    shadow.cpTexSize = (511u & 0x3FFu) | ((255u & 0x3FFu) << 10);
    // GXSetTexCopyDst writes a tile count, not a size. Left as the SDK would
    // leave it, to show it is not what the plan is built from.
    shadow.cpTexStride = (4u & 0x3FFu) | (0x4Du << 24);
    // GXSetColorUpdate(true), GXSetAlphaUpdate(false)
    shadow.cmode0 = (1u << 3);
    // GXSetZMode(compare, func, true)
    shadow.zmode = (1u << 4);

    const CopyPlan plain = decode_copy_plan(shadow);
    if (plain.srcLeft != 64 || plain.srcTop != 32 || plain.srcWidth != 512 || plain.srcHeight != 256) {
        Log.error("self-test: expected source (64,32) 512x256, got ({},{}) {}x{}", plain.srcLeft, plain.srcTop,
            plain.srcWidth, plain.srcHeight);
        return false;
    }
    if (!plain.colorUpdate || plain.alphaUpdate || !plain.depthUpdate) {
        Log.error("self-test: expected masks color=1 alpha=0 depth=1, got color={} alpha={} depth={}",
            plain.colorUpdate, plain.alphaUpdate, plain.depthUpdate);
        return false;
    }
    if (!is_real_copy(plain)) {
        Log.error("self-test: a 512x256 source should count as a real copy");
        return false;
    }

    // The guest's GXCopyTex rewrites zmode's compare bits and cmode0's blend
    // and logicop enables when clearing (GXFrameBuf.c:453-465). None of those
    // is a write mask, so none of them may leak into the plan - a decoder that
    // cleared the blend enables here would be inventing a colour mask the
    // caller never asked for.
    GuestShadow blending = shadow;
    blending.cmode0 |= (1u << 0) | (1u << 1); // blend and logicop enabled
    blending.zmode = 0; // depth write off
    const CopyPlan withBlending = decode_copy_plan(blending);
    if (withBlending.depthUpdate) {
        Log.error("self-test: a copy gained a depth write the caller did not ask for");
        return false;
    }
    if (withBlending.colorUpdate != true || withBlending.alphaUpdate != false) {
        Log.error("self-test: blend and logicop enables must not be read as colour/alpha masks, got "
                  "color={} alpha={}",
            withBlending.colorUpdate, withBlending.alphaUpdate);
        return false;
    }

    // width-1/height-1 of zero is what GXSetTexCopySrc(0, 0, 1, 1) leaves, and
    // it must decode to a 1x1 source rather than to a zero-sized one the report
    // would throw away.
    GuestShadow tiny = shadow;
    tiny.cpTexSize = 0;
    const CopyPlan one = decode_copy_plan(tiny);
    if (one.srcWidth != 1 || one.srcHeight != 1) {
        Log.error("self-test: an all-zero size must decode to 1x1, got {}x{}", one.srcWidth, one.srcHeight);
        return false;
    }

    Log.info("self-test: GXCopyTex copy-rectangle decode matches the SDK's own field packing");
    Log.info("self-test: this checks the decode only - whether a resolved texture is ever bound and "
             "sampled is proved by the copy report of a real run, not here");
    return true;
}

} // namespace sms::recomp::gx_copy
