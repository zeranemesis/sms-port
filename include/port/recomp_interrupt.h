// External-interrupt delivery for the recompiled game.
//
// The DOL contains no exception-vector code: the SDK copies its low-memory
// stubs into place at runtime (__OSExceptionInit), so a *static* recompiler
// never translated anything at 0x500, and nothing in this port ever raised an
// interrupt at all. That is not a small gap - it blocked the boot twice, in
// two unrelated subsystems:
//
//   - The scheduler parked in SelectThread's `while (RunQueueBits == 0) ;`
//     idle loop, because VIWaitForRetrace sleeps until the VI retrace handler
//     increments retraceCount, and nothing ever ran that handler.
//   - The game's own audio driver parked in DSPSendCommands2's
//     `while (Dsp_Running_Check() == 0) ;`, waiting on a flag only a DSP task
//     interrupt callback sets.
//
// The second one is why a narrow workaround is not enough: it is a busy-wait
// in game code, not a scheduler idle point, so there is no "safe moment" to
// special-case. The interrupt has to be deliverable wherever the guest
// happens to be, which means doing what the hardware does - save the
// interrupted context, dispatch, and let the guest resume itself.
//
// That turns out to be tractable, because every piece is a real translated
// function in the dump rather than something this port has to invent:
// __OSDispatchInterrupt (0x8033DF04) and OSLoadContext (0x8033C164). Reading
// __OSDispatchInterrupt settles the shape of it: *every* path through it ends
// in OSLoadContext(context), so it never returns to its caller, and it
// brackets the handler with OSDisableScheduler()/OSEnableScheduler()/
// __OSReschedule() itself. So the host does not need a return path and does
// not need to reason about thread switching: it builds the context, jumps in,
// and the guest's own OSLoadContext rfi's back to wherever it should go.
//
// This module owns the two register blocks that dispatch reads to find out
// what happened - PI's interrupt cause/mask (0xCC003000) and VI's display
// interrupt registers (0xCC002000) - plus the raise/acknowledge bookkeeping
// that connects them.
#pragma once

extern "C" {
#include "cpu/cpu.h"
}

namespace sms::recomp::interrupt {

// Registers the PI and VI MMIO ranges. Call once per CPUState after
// cpu_init(), alongside install_host_calls().
void install();

// PI cause bits, read off __OSDispatchInterrupt's own `if (intsr & ...)`
// chain (src/dolphin/os/OSInterrupt.c) rather than from a datasheet. A device
// that wants to interrupt raises its own status bit in its own registers and
// then raises its PI cause bit here; __OSDispatchInterrupt reads both.
constexpr u32 kCauseVi = 0x00000100u;
constexpr u32 kCauseDsp = 0x00000040u;
constexpr u32 kCauseDi = 0x00000004u;

// Raises or drops PI cause bits. Devices own their own status registers, so
// acknowledging is always two steps: the device clears its own bit, then drops
// its PI cause bit once it has nothing outstanding left.
void raise(u32 causeBits);
void clear(u32 causeBits);

// Flags a vertical retrace as pending: sets the interrupt bit in VI's first
// display-interrupt register, which is what __VIRetraceHandler reads and
// clears, and the VI bit in PI's cause register, which is what
// __OSDispatchInterrupt reads to decide a VI interrupt happened.
//
// Raising is deliberately separate from delivering: this only makes the
// hardware say so, exactly as real hardware would, and dispatch happens when
// the guest is next interruptible.
void raise_vi_retrace(CPUState *cpu);

// True when PI reports a cause that PI's own mask lets through. Mirrors
// __OSDispatchInterrupt's own opening test (`intsr == 0 ||
// (intsr & __PIRegs[1]) == 0`), so the host does not enter dispatch just to
// have it immediately OSLoadContext straight back out.
bool pending(CPUState *cpu);

// Builds an OSContext for the interrupted state at the address
// __OSCurrentContext points to, points the CPU at __OSDispatchInterrupt, and
// returns true. Returns false, changing nothing, when the guest is not in a
// state where an interrupt may be taken - MSR[EE] clear (the guest is inside
// OSDisableInterrupts), or __OSCurrentContext not yet set up.
//
// On success the caller simply keeps running the guest: control does not come
// back here, it comes back through the guest's own OSLoadContext.
bool dispatch(CPUState *cpu);

// PI's cause and mask as they currently stand, for diagnostics only. Whether
// an interrupt is being taken is not visible from the guest's pc, and "stuck
// in the same place" reads identically whether dispatch is firing or never
// firing - these are what tell the two apart.
u32 debug_cause();
u32 debug_mask();

} // namespace sms::recomp::interrupt
