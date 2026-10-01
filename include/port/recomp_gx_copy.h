// Render-to-texture: the GXCopyTex path, and the measurement that says whether
// the game is asking for one at all.
//
// Why this is its own file rather than more of recomp_gx_fifo.cpp: that file
// is being worked on in parallel (uncommitted work adding ~24 GX bridges that
// forward to Aurora and return true). The copy path needs the opposite of
// those - it needs the *guest* body of GXSetTexCopySrc/GXSetTexCopyDst to keep
// running, because those write the copy rectangle into the guest's own GX
// shadow state and push nothing to the FIFO until a copy actually happens. So
// there is no overlap in what the two sets of bridges do, and keeping them in
// separate files means neither has to know the other exists.
//
// The defect this exists to fix (docs/port_todo.md sections 1.2 and 1.3):
// GXCopyTex was never bridged, so g_gxState.copyTextures stayed empty, so any
// surface the game renders into a texture and samples afterwards sampled
// nothing. A texture bound but never resolved looks exactly like a correct
// shader sampling black. The measured corollary was `offscreen=0` across a
// whole session while 512x512 and 1024x1024 passes went to the screen with
// viewport=(4,4 1024x1024) scissor=(0,0 1024x960) - a copy-source pass
// painting the entire framebuffer.
//
// The trap, and why the obvious bridge is wrong: Aurora has GXCopyTex(dest,
// clear) and it reads g_gxState.texCopySrc / texCopyDstWidth / texCopyDstHeight
// / texCopyFmt. Aurora's FIFO command processor decodes no copy-related BP
// register at all - those four fields are only ever set by Aurora's own
// GXSetTexCopySrc and GXSetTexCopyDst, which the recompiled guest never calls.
// So a bridge that simply called ::GXCopyTex would resolve from the previous
// frame's rectangle into the previous frame's dimensions, and would look like
// it worked.
//
// The real state is in the guest's shadow struct, and its offsets are written
// down in libs/dolphin/src/gx/__gx.h. This header also has to stay free of
// Aurora includes, for the same reason recomp_dolphin_sdk.h does: the decode is
// the part worth testing, and testing it needs no disc image, no generated
// code and no graphics stack.
#pragma once

extern "C" {
#include "cpu/cpu.h"
}

#include <cstddef>

namespace sms::recomp::gx_copy {

// Offsets into the guest's GX shadow struct. Every one of these is copied from
// the comment beside the field in libs/dolphin/src/gx/__gx.h, not worked out
// from a disassembly: a wrong offset here produces a plausible copy rectangle
// that happens to be the wrong one, which is indistinguishable from a correct
// bridge reading the wrong thing.
namespace shadow {

// gx is a pointer global at 0x80404628 (orig/GMSP01/files/marioEU.MAP:91916)
// pointing at the gxData object at 0x803fae40, 0x4f8 bytes
// (marioEU.MAP:90826). The pointer is read at run time rather than the object
// address hardcoded, so a disc whose link order differs fails loudly on the
// bounds check instead of quietly decoding from the wrong place.
constexpr u32 kGuestGxPointer = 0x80404628u;
constexpr u32 kGuestGxDataSize = 0x4F8u;

constexpr u32 kCmode0 = 0x1D0; // bit 3 color update, bit 4 alpha update
constexpr u32 kCmode1 = 0x1D4; // dst alpha / a-mode
constexpr u32 kZmode = 0x1D8; // bit 4 depth update
constexpr u32 kPeCtrl = 0x1DC;
constexpr u32 kCpTexSrc = 0x1F0; // left bits 0-9, top bits 10-19
constexpr u32 kCpTexSize = 0x1F4; // width-1 bits 0-9, height-1 bits 10-19
constexpr u32 kCpTexStride = 0x1F8; // row tiles, not a size - see CopyPlan
constexpr u32 kCpTex = 0x1FC; // dest format bits 4-6, high bit 3, mipmap bit 9
constexpr u32 kCpTexZ = 0x200;
constexpr u32 kVpLeft = 0x43C;
constexpr u32 kVpTop = 0x440;
constexpr u32 kVpWd = 0x444;
constexpr u32 kVpHt = 0x448;

// The guest's GXTexObj, as recomp_gx_fifo.cpp's emit_guest_texture_metadata
// already reads it. Copied here rather than shared so this file stays
// independent of the one being edited in parallel.
constexpr u32 kGuestTexObjSize = 0x20;
constexpr u32 kGuestTexObjImage0 = 0x08; // width-1 bits 0-9, height-1 bits 10-19
constexpr u32 kGuestTexObjImage3 = 0x0C; // texture address, 21 bits << 5
constexpr u32 kGuestTexObjFormat = 0x14;
constexpr u32 kGuestTexObjTlut = 0x18;

} // namespace shadow

// The handful of gxData words the copy path needs, pulled out of guest memory
// as plain values. Passing these rather than a CPUState pointer is what lets
// run_gx_copy_self_test() check the decode with nothing else present - the
// same seam format_os_report() uses in recomp_dolphin_sdk.h.
struct GuestShadow {
    u32 cmode0 = 0;
    u32 cmode1 = 0;
    u32 zmode = 0;
    u32 peCtrl = 0;
    u32 cpTexSrc = 0;
    u32 cpTexSize = 0;
    u32 cpTexStride = 0;
    u32 cpTex = 0;
    u32 cpTexZ = 0;
    float vpLeft = 0.0f;
    float vpTop = 0.0f;
    float vpWd = 0.0f;
    float vpHt = 0.0f;
};

// What one GXCopyTex will do, in the terms Aurora's GXCopyTex wants.
struct CopyPlan {
    u32 srcLeft = 0;
    u32 srcTop = 0;
    u32 srcWidth = 0;
    u32 srcHeight = 0;
    // Deliberately absent: the destination's own width, height and format.
    // GXSetTexCopyDst stores those only in cpTexStride (a tile count) and the
    // 3-bit format field, which is not enough to recover the size, and they
    // are fully present in the guest GXTexObj the call was handed. The bridge
    // therefore takes the destination from the object and the source from the
    // shadow, which is where each half actually comes from.
    bool colorUpdate = true;
    bool alphaUpdate = true;
    bool depthUpdate = true;
};

// Decodes the copy rectangle and the three write masks from a guest shadow
// image. Pure: no guest memory, no Aurora, no logging, and inline here rather
// than in the .cpp for exactly that reason - it is the part of this file worth
// testing, and testing it should not need the graphics stack.
//
// Bit positions are the SDK's own: zmode bit 4 for depth update (GXSetZMode,
// GXPixel.c:173), cmode0 bit 3 for colour (GXSetColorUpdate, GXPixel.c:150)
// and bit 4 for alpha (:159). Depth is *not* a cmode0 bit - reading it as one
// is the kind of plausible-but-wrong copy this file exists to avoid.
//
// No `clear` argument, and that is a result rather than an omission. The
// guest's GXCopyTex does rewrite its shadow state when clearing
// (GXFrameBuf.c:453-465): zmode bit 0 (ZCompare enable) and bits 1-3 (the
// compare function) get set, and cmode0's blend and logicop enables get
// cleared. None of those five bits is one the copy resolve reads - it reads
// colour/alpha update from cmode0 bits 3-4 and depth update from zmode bit 4,
// and the clear path leaves all three alone. So the rewrite changes nothing
// the resolve can see, and taking a `clear` flag here would have meant
// carrying a parameter that provably has no effect.
inline CopyPlan decode_copy_plan(const GuestShadow &shadow)
{
    CopyPlan plan;

    // GXSetTexCopySrc packs left and top into cpTexSrc (GXFrameBuf.c:106-109)
    // and width-1/height-1 into cpTexSize (:111-114). The +1 is the SDK's, not
    // this decoder's invention.
    plan.srcLeft = shadow.cpTexSrc & 0x3FFu;
    plan.srcTop = (shadow.cpTexSrc >> 10) & 0x3FFu;
    plan.srcWidth = (shadow.cpTexSize & 0x3FFu) + 1;
    plan.srcHeight = ((shadow.cpTexSize >> 10) & 0x3FFu) + 1;

    plan.colorUpdate = (shadow.cmode0 & (1u << 3)) != 0;
    plan.alphaUpdate = (shadow.cmode0 & (1u << 4)) != 0;
    plan.depthUpdate = (shadow.zmode & (1u << 4)) != 0;

    return plan;
}

// True when the decoded plan is a real copy rather than a degenerate one - a
// non-empty source rectangle. Used by the per-second report to separate passes
// that resolve a texture from calls that resolve a zero-sized nothing.
inline bool is_real_copy(const CopyPlan &plan)
{
    return plan.srcWidth > 0 && plan.srcHeight > 0;
}

// Registers the copy-path observers and the GXCopyTex bridge by name.
// `addresses` maps a symbol name (as in generated/generated_symbols.h's
// DOLRECOMP_SYMBOL_<name>) to the address dolrecomp resolved for this disc. A
// name with no trampoline here is skipped silently: coverage is expected to
// trail the SDK's symbol list, the same contract as every other register_known_*
// in this port.
struct NamedAddress {
    const char *name;
    u32 address;
};
void register_known_gx_copy_calls(const NamedAddress *addresses, size_t count);

// Checks decode_copy_plan() and is_real_copy() against a synthetic shadow
// image shaped exactly as the SDK's own GXSetTexCopySrc/GXSetTexCopyDst would
// have left it, including the clear-path rewrite of zmode and cmode0.
//
// What it does NOT check, and says so in its own log line: that a resolved
// texture is ever bound or sampled. That needs a real frame, and a decode
// that passes here can still be paired with the wrong destination pointer. The
// proof of the bridge is the per-second copy report the running port emits, not
// this test.
bool run_gx_copy_self_test();

} // namespace sms::recomp::gx_copy
