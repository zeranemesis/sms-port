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

// The shared sub-word read/write the PI, VI and PE shadows need. Those blocks
// are accessed as halfwords by the SDK (`u16* __VIRegs`, `u16* __peReg`) and as
// words by others, so the containing big-endian word is the unit that is
// stored and the access picks bytes out of it.
//
// Inline here rather than in the .cpp because the lane arithmetic is the easy
// thing to get subtly wrong - a status bit that lands in a neighbouring
// register reads back as "nothing happened" - and the only way that gets caught
// before a run is if it can be tested. run_pe_lane_self_test() does exactly
// that.
inline u32 extract(u32 word, u32 addr, u8 size)
{
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    return (word >> shift) & ((size == 4) ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1));
}

inline void insert(u32 &word, u32 addr, u64 value, u8 size)
{
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u32 mask = (size == 4 ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1)) << shift;
    word = (word & ~mask) | ((static_cast<u32>(value) << shift) & mask);
}

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

// The pixel engine's two. Read off __OSDispatchInterrupt's own cause chain
// (src/dolphin/os/OSInterrupt.c:368 and :371), which maps 0x400 to
// OS_INTERRUPTMASK_PI_PE_FINISH and 0x200 to OS_INTERRUPTMASK_PI_PE_TOKEN -
// the two handlers __GXPEInit installs at interrupt 0x13 and 0x12
// (src/dolphin/src/gx/GXMisc.c:263-270). kCausePeFinish is the one that matters
// for play: it is what wakes GXWaitDrawDone.
constexpr u32 kCausePeFinish = 0x00000400u;
constexpr u32 kCausePeToken = 0x00000200u;

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

// Flags a pixel-engine finish as pending, the way the hardware does when the
// GP drains: sets the finish status bit in the PE block's control register,
// which is what GXFinishInterruptHandler reads, and raises the PI cause bit,
// which is what __OSDispatchInterrupt reads to decide a finish happened.
//
// Raised once per presented guest frame, alongside raise_vi_retrace: a
// retrace happens after the GP has finished the frame, so that is the point
// where "the GPU is done" is a true statement. Raising it any earlier would
// report a finish for work Aurora has not submitted yet.
//
// The guest clears it by writing the status bit back (GXFinishInterruptHandler
// sets __peReg[5] bit 3, GXMisc.c:239), and that write is what drops the cause
// again - so a frame whose finish is never acknowledged keeps reporting, which
// is the honest outcome for a host that is genuinely behind.
void raise_pe_finish();

// Installs the DEC SPR callbacks and begins tracking the GameCube
// decrementer.  The SDK's OSAlarm code programs DEC through mtspr 22; unlike
// PI devices this is a CPU exception, so expiry enters the guest's registered
// decrementer exception handler directly.
void install_decrementer(CPUState *cpu);

// Accounts for guest timebase ticks that have elapsed since the previous
// call.  Returns true when expiry transferred control to the guest exception
// handler.  Call only at a translated-block boundary, where the host owns the
// CPUState and can safely inject an asynchronous exception.
bool advance_decrementer(CPUState *cpu, u64 elapsedTicks);

// Exercises DEC's SPR wiring, countdown and exception handoff without
// requiring a disc image or a running Aurora window.
bool run_decrementer_self_test();

// Checks that a pixel-engine finish survives the trip through the block's own
// halfword addressing, and that the guest's acknowledge brings the PI cause
// back down.
//
// This exists because the alternative is a status bit that quietly lands in a
// neighbouring register: it reads back as "nothing happened", the cause never
// clears, and the symptom is an interrupt that re-fires every frame. It needs
// no disc image, no generated code and no Aurora - dr_cpu has none of those
// either.
bool run_pe_lane_self_test();

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

// True when the guest currently has MSR[EE] set, i.e. when an interrupt could
// actually be taken. This is not the same question as "is anything pending":
// a guest sitting in a spin with interrupts disabled still has work to do -
// the instruction that re-enables them - and must be run, not skipped.
bool interrupts_enabled(CPUState *cpu);

// PI's cause and mask as they currently stand, for diagnostics only. Whether
// an interrupt is being taken is not visible from the guest's pc, and "stuck
// in the same place" reads identically whether dispatch is firing or never
// firing - these are what tell the two apart.
u32 debug_cause();
u32 debug_mask();

} // namespace sms::recomp::interrupt
