// Bridges the GX write-gather-pipe (the real GameCube hardware mechanism
// GX commands are streamed through - physical address 0xCC008000) and the
// handful of GX control-plane SDK calls that configure it.
//
// Unlike OSReport-style calls, most GX vertex/attribute functions
// (GXPosition3f32, GXColor4u8, ...) never reach host_call at all: their
// translated bodies just do real stores to 0xCC008000, which dr_cpu routes
// through CPUState::external_write once installed (see
// include/port/recomp_host.h's MmioRangeHandler) - no per-function bridge
// needed for those. Aurora already has a full GX command decoder
// (aurora::gx::fifo::process, extern/aurora/lib/gx/command_processor.cpp)
// consuming its own software FIFO buffer (extern/aurora/lib/gx/fifo.hpp);
// this file only has to forward raw bytes into that existing buffer, not
// reimplement any GX command decoding.
#pragma once

extern "C" {
#include "cpu/cpu.h"
}

#include <cstddef>

namespace sms::recomp::gx_fifo {

// Registers the external_write handler for [0xCC008000, 0xCC009000) that
// forwards every store in range to aurora::gx::fifo::write_u8/u16/u32.
// Aurora already drains and processes that buffer once per host frame
// unconditionally (extern/aurora/lib/aurora.cpp's end_frame()), so nothing
// here needs to drive draining itself.
void install();

// Gives Aurora the guest-memory translation it needs for the CP array-base
// registers (0xA0-0xAF), which hold a guest physical address it has no way to
// read. Without it those registers are refused outright and every piece of
// indexed geometry is drawn with no vertex data - measured as 351,846
// rejections in one 70-second gameplay run. Bridging GXSetArray is not enough
// on its own, because the guest re-sends these registers from its own shadow
// state on every dirty-state flush.
void install_array_base_resolver(CPUState *cpu);

// Bridges GXInit/GXSetCPUFifo/GXSetGPFifo/GXSetDrawDone/GXDrawDone/
// GXFlush/GXCopyDisp/GXSetArray/GXLoadTexObj/GXLoadTexObjPreLoaded/
// GXInvalidateTexAll - the starting set discovered
// to matter so far, not a claimed-complete enumeration (see
// include/port/recomp_dolphin_sdk.h's NamedAddress for the shape this
// mirrors).
struct NamedAddress {
    const char *name;
    u32 address;
};
void register_known_gx_calls(const NamedAddress *addresses, size_t count);

// Regression test for the FIFO cut this port produces by construction.
//
// step_game() runs the guest for a fixed budget of translated blocks and stops
// at whatever instruction that budget expires on, which can fall between a GX
// command's opcode and its payload. Aurora's drain() then used to process that
// buffer whole and abort - measured twice as "draw vertex data overrun: need 80
// bytes at pos N, have N", at two unrelated FIFO positions but with
// byte-identical preceding commands, so a cut rather than corruption.
//
// Deterministic, unlike the crash it stands in for: it feeds process_stream a
// buffer that ends mid-command and checks the incomplete tail is reported
// rather than consumed, then that completing it processes the whole thing.
//
// It exercises the carry-over mechanism, not the draw path specifically -
// a draw needs live GX and graphics state that a headless test has no way to
// stand up honestly.
bool run_fifo_stream_self_test();

} // namespace sms::recomp::gx_fifo
