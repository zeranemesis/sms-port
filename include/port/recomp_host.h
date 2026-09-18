// The bridge between DolRecomp's generated code and Aurora.
//
// DolRecomp's plain C backend (`--backend c`, no LLVM, no ModernGekko - see
// docs/recompilation.md's "DolRecomp runtime contract" section) emits one
// `void func_<address>(CPUState* ctx)` per recompiled function operating on
// a CPUState register file (extern/dolrecomp/src/cpu/cpu.h). When generated
// code calls an address DolRecomp has no native translation for - a Dolphin
// SDK library function such as GXSetVtxDesc or PADRead, identified by its
// address from the game's linker MAP - CPUState::host_call fires with that
// address. This header is the dispatch table for that hook: everything the
// game calls into hardware/OS/GX/PAD/etc through, per Aurora's own headers
// under extern/aurora/include/dolphin, gets a HostCallEntry mapping the
// guest address to a trampoline that marshals CPUState's r3-r10 into a real
// call into Aurora and writes the result back to r3.
//
// No entries are registered yet: they are keyed by runtime address, and an
// address only exists once dolrecomp has run against a real GMSP01 dol/map
// (see tools/port/recompile.py). Until then this is dispatch-table
// infrastructure plus an unresolved-call logger - see run_self_test() for
// what is actually exercised today.
#pragma once

// extern/dolrecomp's cpu.h has no extern "C" guard of its own (unlike
// Aurora's dolphin/*.h headers) - wrap it here, once, rather than at every
// include site. Without this, CPUState's C linkage in the C-compiled
// dr_cpu library mismatches the C++-mangled names this header would
// otherwise declare, and every dr_cpu symbol fails to link.
extern "C" {
#include "cpu/cpu.h"
}

#include <cstddef>

namespace sms::recomp {

using HostCallFn = bool (*)(CPUState *cpu, u32 address);

struct HostCallEntry {
    u32 address;
    const char *name;
    HostCallFn fn;
};

// Adds entries to the dispatch table. Safe to call more than once; a later
// entry for an address already registered replaces it (the last map-derived
// binding pass wins over any earlier, possibly stale, one).
void register_host_calls(const HostCallEntry *entries, size_t count);

// Points cpu->host_call at this module's dispatcher. Call once per CPUState
// after cpu_init(). The dispatcher looks the address up in the registered
// table; on a miss it logs the guest address and the calling PC (cpu->pc)
// and returns false, which is exactly the signal that table needs another
// entry - the intended workflow once real addresses exist.
void install_host_calls(CPUState *cpu);

// Exercises the dispatch table and the unresolved-call path against a
// CPUState this module owns end to end - no generated game code or disc
// image needed, since dr_cpu itself has none. Does not exercise a single
// real Dolphin SDK trampoline, because none can exist without a real
// address from a real MAP file.
bool run_self_test();

} // namespace sms::recomp
