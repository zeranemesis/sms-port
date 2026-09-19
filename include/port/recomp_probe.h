// A read-only window into the running guest's own OS and game state.
//
// The heartbeat in portmain.cpp says *where* the guest's pc is, which located
// every boot blocker so far. It stops being enough the moment the answer is
// "in the scheduler's idle loop": that is where a healthy guest waits, so it
// names the symptom and hides the cause. The cause is always some other
// thread, blocked on something, and this is what reads that out.
//
// Everything here dereferences guest memory at fixed addresses taken from
// config/GMSP01/symbols.txt (which, unlike generated/generated_symbols.h, has
// the *data* symbols) and from the decompiled struct definitions. Every
// constant below cites where it came from, because an address that is merely
// plausible would produce confident nonsense.
//
// This is diagnostics, not emulation: it writes nothing, and the guest cannot
// tell it ran.
#pragma once

extern "C" {
#include "cpu/cpu.h"
}

namespace sms::recomp::probe {

// Dumps the guest's progression gates, DVD-layer state and thread list. Call
// once per frame; it throttles itself to roughly one report per second and
// stops entirely after a fixed number of reports, so it cannot become the
// sixth runaway log this project has had.
void report(CPUState *cpu);

} // namespace sms::recomp::probe
