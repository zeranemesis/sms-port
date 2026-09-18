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

// Bridges GXInit/GXSetCPUFifo/GXSetGPFifo/GXSetDrawDone/GXDrawDone/
// GXFlush/GXCopyDisp - the starting set discovered to matter so far, not a
// claimed-complete enumeration (see include/port/recomp_dolphin_sdk.h's
// NamedAddress for the shape this mirrors).
struct NamedAddress {
    const char *name;
    u32 address;
};
void register_known_gx_calls(const NamedAddress *addresses, size_t count);

} // namespace sms::recomp::gx_fifo
