// Loads the recompiled game (docs/recompilation.md) from the disc image
// configured in Settings -> Prelaunch -> Disc Image and drives it, once
// generated/ exists (see include/port/recomp_host.h and
// include/port/recomp_dolphin_sdk.h for the mechanism this sits on top of).
//
// Without a real GMSP01 dump, generated/ is empty and
// DOLPHINJET_HAVE_RECOMPILED_GAME is not defined (see CMakeLists.txt) - in
// that case both functions below are trivial stubs that return false, so
// this header stays includable (and portmain.cpp stays buildable) on a
// fresh checkout and in CI exactly as before.
#pragma once

extern "C" {
#include "cpu/cpu.h"
}

namespace sms::recomp {

// Requires a disc image already opened via aurora_dvd_open() + DVDInit()
// (see docs/port_bootstrap.md's "What actually needs a disc image from
// here"). Reads the DOL Aurora's DVD layer exposes via DVDGetDOLLocation
// (the raw bytes nod already pulled off the mounted disc - no
// "sys/main.dol" path lookup needed), checks the disc's game ID is GMSP01
// (the dump generated/ was produced from - booting a different disc image
// against it would silently run the wrong addresses), loads its text/data
// sections into a freshly cpu_init'd *cpu, installs the host-call bridge
// and the known Dolphin SDK trampolines, and sets cpu->pc to the DOL's own
// entry point. Returns false, logged with the reason, on any failure - no
// disc mounted, wrong game ID, out-of-range section, cpu_init failure.
// On success, cpu owns heap state (CPUState::ram) that must eventually be
// released with cpu_free().
bool boot_game(CPUState *cpu);

// Runs up to maxBlocks translated chunks (generated.h's
// dolrecomp_run_blocks) starting from cpu->pc. Call once per frame after a
// successful boot_game() - the block budget keeps one guest frame from
// blocking the host's own render loop indefinitely. Returns false once the
// guest can't continue (a CPU exception, or an unresolved host call - see
// recomp_host.h) - logged with the last pc, not fatal to the caller, which
// stays free to keep presenting the menu/overlay on top.
bool step_game(CPUState *cpu, unsigned maxBlocks);

// Diagnostic for the horizontal noise bands: reports whether every byte of
// [guestAddr, guestAddr + bytes) was written by a locked-cache store DMA that
// is still in the recent-transfer ring (see perform_locked_cache_dma).
//
// The THP decoder's only route out of the locked cache is LCStoreData, one
// call per macroblock row (THPDec.c:1587-1592), so a plane row that no store
// ever covered was never delivered and still holds whatever the buffer had
// before - which is exactly what a band of random-byte noise is. Asking this
// about a rough row AND a clean row in the same frame is what separates "the
// DMA missed it" from "the DMA delivered garbage", and only the second would
// put the fault upstream in the IDCT.
//
// The ring is finite, so a false answer for an old plane means "not recently
// covered", not "never covered"; recentEnough says whether the ring still
// reaches back past the plane at all.
bool locked_cache_store_covered(u32 guestAddr, u32 bytes, bool *recentEnough);

// Exercises the emulated 16 KiB Gekko locked cache and its two DMA directions
// without a disc image or generated game code.  THP relies on this exact path
// to move decoded YUV rows from 0xE0000000 into guest RAM.
bool run_locked_cache_self_test();

// Verifies the port-side lazy floating-point context handoff used when guest
// OS threads first execute an FP or paired-single instruction.
bool run_fpu_context_self_test();

} // namespace sms::recomp
