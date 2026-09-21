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
#include <string>

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

// host_call (above) fires when translated code jumps to an address outside
// the translated ranges - it has nothing to do with plain memory
// loads/stores to an address outside guest RAM (real GameCube hardware
// registers: GX's write-gather pipe, EXI, SI, VI...). dr_cpu already routes
// those through CPUState::external_read/external_write when resolve_addr()
// can't map them into RAM (extern/dolrecomp/src/cpu/cpu.c's mem_read*/
// mem_write*) - this is that second, address-*range*-keyed dispatch table,
// parallel to the address-*exact*-keyed one above. Each hardware area (EXI,
// GX FIFO, later SI/PAD/VI) registers its own range here instead of this
// file growing hardware-specific knowledge.
using ExternalReadFn = u64 (*)(CPUState *cpu, u32 addr, u8 size);
using ExternalWriteFn = void (*)(CPUState *cpu, u32 addr, u64 value, u8 size);

struct MmioRangeHandler {
    u32 base; // inclusive
    u32 end; // exclusive
    const char *name;
    ExternalReadFn read; // nullable - a miss/null read reads as zero
    ExternalWriteFn write; // nullable - a miss/null write is dropped
};

// Registers a handler for [handler.base, handler.end). Safe to call more
// than once; a newly-registered range that overlaps one already registered
// is rejected (logged) rather than silently shadowed, since two handlers
// both matching the same address is a real bug, not something to paper
// over silently.
void register_mmio_range(const MmioRangeHandler &handler);

// Points cpu->external_read/external_write at this module's range
// dispatcher. Call once per CPUState after cpu_init(), alongside
// install_host_calls(). A read/write that matches no registered range logs
// once and falls back to the same "unmapped" behavior dr_cpu's own
// mem_read*/mem_write* already have without this installed (read as zero,
// write dropped) - so a not-yet-discovered hardware range degrades exactly
// like it did before this table existed, rather than going unhandled.
void install_external_memory(CPUState *cpu);

// Exercises register_mmio_range/install_external_memory end to end (a hit
// on a registered range, a miss outside any range) against a CPUState this
// module owns - no generated game code or disc image needed.
bool run_mmio_self_test();

// --- guest clock ------------------------------------------------------------
//
// CPUState::timebase is what the guest's OSGetTime()/mftb reads, and dr_cpu
// never advances it on its own (extern/dolrecomp/src/cpu/cpu.c's ppc_mftb
// just returns the field). It used to be advanced once per host frame from
// the host's wall clock, which gets the long-run rate right and the
// resolution catastrophically wrong: inside a frame guest time stood still,
// and between frames it jumped ~16.6ms.
//
// That is fatal to the SDK's hardware-calibration loops, which poll a
// register and time the gap with OSGetTime() in microseconds.
// __AI_SRC_INIT (src/dolphin/ai/ai.c) is the one that caught it: it accepts
// a gap below 28.5us or between 34.5us and 39.0us and spins forever on
// anything else, so a frame-granular clock could only ever report ~16.6ms
// and could never finish.
//
// So guest time is derived from guest execution instead: the core runs at 12
// timebase ticks' worth of cycles per tick (486MHz core, 40.5MHz
// OS_TIMER_CLOCK), and CPUState::downcount is the cycle counter generated
// code already decrements. sync_guest_timebase() converts whatever has been
// consumed since the last call into ticks, keeping the sub-tick remainder so
// nothing is lost to truncation, and is called at every MMIO access - which
// is exactly where a polling loop observes the world, so the timebase the
// guest reads a few instructions later is accurate to those instructions.
//
// rebase_guest_timebase() declares "downcount just changed for a reason that
// is not execution" (a budget refill, a restored CPUState) so that the jump
// is not mistaken for consumed cycles.
void sync_guest_timebase(CPUState *cpu);
void rebase_guest_timebase(CPUState *cpu);

// Reads a NUL-terminated string out of guest memory, shared by every
// trampoline that takes a guest `const char*` (OSReport's %s, CARDInit's
// game/maker strings, ...) - factored out of recomp_dolphin_sdk.cpp, which
// had its own private copy of this exact loop.
std::string read_guest_cstring(CPUState *cpu, u32 addr, size_t maxLen = 4096);

} // namespace sms::recomp
