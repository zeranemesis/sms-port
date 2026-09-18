// Bridges for GameCube EXI (External Interface - memory card / GBA link /
// serial device bus) hardware and SDK calls.
//
// Unlike OSReport, Aurora has no real EXI implementation to forward to -
// extern/aurora/include/dolphin/exi.h only declares the API, with no .cpp
// anywhere in extern/aurora backing it (verified). So everything here is a
// hand-written "no device attached" stub, not a bridge to a real
// implementation - the goal is only to let the game's own EXI probing
// finish (as "nothing here") instead of spinning forever, not to emulate
// real EXI hardware behavior.
#pragma once

extern "C" {
#include "cpu/cpu.h"
}

#include <cstddef>

namespace sms::recomp::exi {

// Registers the MmioRangeHandler(s) (see include/port/recomp_host.h) for
// the EXI hardware register space. This is the mechanism that actually
// unblocks the confirmed boot hang (docs/port_bootstrap.md's "Booted
// against a real GMSP01 dump" item 7): the hang is plain inline lhz/lwz
// loads inside EXIProbe's own translated body, reached as a native call
// before any host_call mechanism ever sees it - so only an
// external_read/external_write hook, not a function-call bridge, can
// intercept it. The exact base/size covered here is a starting guess (one
// GameCube EXI channel's register block, CSR/MAR/Length/CR/Data, is a
// handful of 32-bit registers) meant to be widened/narrowed once a real
// run's log shows exactly which offsets are touched.
void install();

// The starting set of EXI SDK entry points safe to stub unconditionally
// (see include/port/recomp_dolphin_sdk.h's NamedAddress for the shape this
// mirrors). Cheap, harmless coverage independent of install() above - only
// useful for whichever EXI entry points do turn out to be reached as
// host_call misses rather than purely through their own translated body.
struct NamedAddress {
    const char *name;
    u32 address;
};
void register_known_exi_calls(const NamedAddress *addresses, size_t count);

} // namespace sms::recomp::exi
