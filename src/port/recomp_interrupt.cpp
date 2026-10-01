#include "port/recomp_interrupt.h"

#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"

#include <array>
#include <cstring>

// generated_symbols.h has no extern "C" guard of its own - see
// recomp_boot.cpp's comment for why that matters from a .cpp file.
#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME
extern "C" {
#include "generated_symbols.h"
}
#endif

namespace sms::recomp::interrupt {
namespace {

aurora::Module Log("sms::recomp::interrupt");

// --- PI: interrupt cause and mask -------------------------------------------
//
// src/dolphin/os/OSInit.c points `__piReg` at OSPhysicalToUncached(0xC003000),
// and __OSDispatchInterrupt reads __PIRegs[0] (cause) and __PIRegs[1] (mask).
// SetInterruptMask (OSInterrupt.c:244) is what writes the mask, ORing in
// 0x100 for PI_VI - so the guest programs this itself and the host only has
// to remember it. Left unmapped, the mask read back as 0 and dispatch would
// have bailed out immediately on `(intsr & __PIRegs[1]) == 0`.
constexpr u32 kPiBase = 0xCC003000u;
constexpr u32 kPiEnd = 0xCC003040u;
constexpr u32 kPiCauseOffset = 0x00u;
constexpr u32 kPiMaskOffset = 0x04u;

// __OSDispatchInterrupt opens with `intsr &= ~0x00010000;` - that bit is not a cause and is
// masked out before anything looks at it, so nothing here ever sets it.

std::array<u32, (kPiEnd - kPiBase) / 4> g_piShadow {};

// --- VI: display interrupt registers ----------------------------------------
//
// `__VIRegs` is a u16*, so __VIRetraceHandler's indices 0x18/0x1A/0x1C/0x1E
// are byte offsets 0x30/0x34/0x38/0x3C - the high halves of the four display
// interrupt registers, whose top bit (0x8000 of that halfword) is the
// interrupt status. The handler tests each one, clears the ones it finds set,
// and treats the last two as mid-frame interrupts it returns early from; only
// the first two reach the retraceCount++ / OSWakeupThread(&retraceQueue) path.
constexpr u32 kViBase = 0xCC002000u;
constexpr u32 kViEnd = 0xCC002080u;
constexpr u32 kViDisplayInterrupt0Offset = 0x30u;

// Bit 31 of the 32-bit register, i.e. bit 15 of the halfword at offset 0x30,
// which is what `__VIRegs[0x18] & 0x8000` tests.
constexpr u32 kViInterruptBit = 0x80000000u;

std::array<u32, (kViEnd - kViBase) / 4> g_viShadow {};

// Every display-interrupt register, so acknowledging can ask "is any VI
// interrupt still outstanding?" rather than assuming only the first is ever
// used.
constexpr u32 kViDisplayInterruptOffsets[] = { 0x30u, 0x34u, 0x38u, 0x3Cu };

// The shared sub-word read/write these shadows need now live in
// include/port/recomp_interrupt.h, inline, so run_pe_lane_self_test() can use
// the same implementation rather than a copy of it.

u64 pi_read(CPUState *, u32 addr, u8 size)
{
    const size_t index = ((addr & ~3u) - kPiBase) / 4;
    const u32 word = index < g_piShadow.size() ? g_piShadow[index] : 0;
    return extract(word, addr, size);
}

void pi_write(CPUState *, u32 addr, u64 value, u8 size)
{
    const size_t index = ((addr & ~3u) - kPiBase) / 4;
    if (index >= g_piShadow.size()) {
        return;
    }
    insert(g_piShadow[index], addr, value, size);
}

u64 vi_read(CPUState *, u32 addr, u8 size)
{
    const size_t index = ((addr & ~3u) - kViBase) / 4;
    const u32 word = index < g_viShadow.size() ? g_viShadow[index] : 0;
    return extract(word, addr, size);
}

void vi_write(CPUState *, u32 addr, u64 value, u8 size)
{
    const size_t index = ((addr & ~3u) - kViBase) / 4;
    if (index >= g_viShadow.size()) {
        return;
    }
    insert(g_viShadow[index], addr, value, size);

    // The handler acknowledges by clearing the status bit it found set. Once
    // none is left outstanding, PI must stop reporting a VI cause, or dispatch
    // would fire again on the same interrupt forever.
    bool anyOutstanding = false;
    for (const u32 offset : kViDisplayInterruptOffsets) {
        if ((g_viShadow[offset / 4] & kViInterruptBit) != 0) {
            anyOutstanding = true;
            break;
        }
    }
    if (!anyOutstanding) {
        g_piShadow[kPiCauseOffset / 4] &= ~kCauseVi;
    }
}

// --- PE: pixel engine token and finish ---------------------------------------
//
// The block __peReg points at: `__peReg = OSPhysicalToUncached(0xC001000)`
// (libs/dolphin/src/gx/GXInit.c:79), the uncached alias of 0xCC001000.
// __peReg is a u16*, so an index is twice the byte offset.
//
// __GXPEInit (libs/dolphin/src/gx/GXMisc.c:263-270) is what gives the two
// handlers this emulates their meaning, and it is the whole contract:
//
//     __OSSetInterruptHandler(0x12, GXTokenInterruptHandler);
//     __OSSetInterruptHandler(0x13, GXFinishInterruptHandler);
//     __OSUnmaskInterrupts(0x2000);
//     __OSUnmaskInterrupts(0x1000);
//
// 0x12 and 0x13 are __OS_INTERRUPT_PI_PE_TOKEN and __OS_INTERRUPT_PI_PE_FINISH
// (OSInterrupt.h:33-34), and their PI cause bits are 0x00000200 and 0x00000400
// (OSInterrupt.c:371 and :368). Address, cause bits and both handlers are all
// readable off the tree, which is why this needs no datasheet and no guess.
//
// The acknowledge direction is the opposite of the VI's, and that is the part
// worth getting right: vi_write drops the PI cause when the guest *clears* the
// status bit, because __VIRetraceHandler clears it. GXFinishInterruptHandler
// *sets* __peReg[5] bit 3 and GXTokenInterruptHandler *sets* bit 2
// (GXMisc.c:239 and :232). So the clear hangs off a write that raises the bit,
// not off a read or a clearing write.
//
// What a finish means to the guest: GXFinishInterruptHandler sets DrawDone = 1,
// runs DrawDoneCB and wakes FinishQueue (GXMisc.c:242-260), which is what
// GXWaitDrawDone is parked on (GXMisc.c:93). Before this block existed the port
// bridged GXSetDrawDone away and forced the DrawDone flag, so the flag never
// cycled and a genuinely late GP stayed invisible - the defect
// docs/port_todo.md section 3.1 records.
constexpr u32 kPeBase = 0xCC001000u;
constexpr u32 kPeEnd = 0xCC001020u;
constexpr u32 kPeControlOffset = 0x0Au; // __peReg[5]
constexpr u32 kPeControlTokenBit = 1u << 2; // set by GXTokenInterruptHandler
constexpr u32 kPeControlFinishBit = 1u << 3; // set by GXFinishInterruptHandler

// PE_TOKEN (cause 0x200) is deliberately never raised. Its handler
// (GXTokenInterruptHandler) exists to pass a draw-sync token to TokenCB, and
// the token is produced when a GXSetDrawSync write reaches the GPU - a point
// that lives inside the FIFO flush path, in recomp_gx_fifo.cpp, which is being
// worked on in parallel. Raising it from the frame boundary instead would
// deliver a callback with a token nobody wrote. The token register itself
// (__peReg[7], byte offset 0x0E) is covered by the shadow below, so a guest
// write to it lands and reads back rather than being logged as unmapped.

std::array<u32, (kPeEnd - kPeBase) / 4> g_peShadow {};

u64 pe_read(CPUState *, u32 addr, u8 size)
{
    const size_t index = ((addr & ~3u) - kPeBase) / 4;
    const u32 word = index < g_peShadow.size() ? g_peShadow[index] : 0;
    return extract(word, addr, size);
}

void pe_write(CPUState *, u32 addr, u64 value, u8 size)
{
    const size_t index = ((addr & ~3u) - kPeBase) / 4;
    if (index >= g_peShadow.size()) {
        return;
    }
    insert(g_peShadow[index], addr, value, size);

    // The guest acknowledges by setting its own handler's status bit, so a
    // write here that raises one *is* the acknowledgement. Drop the PI cause
    // once the bit is set, or dispatch would fire on it forever.
    const u32 control = extract(g_peShadow[kPeControlOffset / 4], kPeBase + kPeControlOffset, 2);
    if ((control & kPeControlFinishBit) != 0) {
        g_piShadow[kPiCauseOffset / 4] &= ~kCausePeFinish;
    }
    if ((control & kPeControlTokenBit) != 0) {
        g_piShadow[kPiCauseOffset / 4] &= ~kCausePeToken;
    }
}

#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME

// src/dolphin/os/OSContext.c: `__OSCurrentContext AT_ADDRESS(OS_BASE_CACHED |
// 0x00D4)`.
constexpr u32 kOSCurrentContextAddress = 0x800000D4u;

// include/dolphin/os/OSContext.h's own OS_CONTEXT_* offsets. Spelled out here
// because this writes the struct into guest memory by hand rather than
// through a C declaration of it.
constexpr u32 kContextGpr = 0;     // gpr[32], 4 bytes each
constexpr u32 kContextCr = 128;
constexpr u32 kContextLr = 132;
constexpr u32 kContextCtr = 136;
constexpr u32 kContextXer = 140;
constexpr u32 kContextFpr = 144;   // fpr[32], 8 bytes each
constexpr u32 kContextFpscrPad = 400;
constexpr u32 kContextFpscr = 404;
constexpr u32 kContextSrr0 = 408;
constexpr u32 kContextSrr1 = 412;
constexpr u32 kContextMode = 416;  // u16
constexpr u32 kContextState = 418; // u16
constexpr u32 kContextGqr = 420;   // gqr[8], 4 bytes each
constexpr u32 kContextPsf = 456;   // ps1[32], 8 bytes each

// OSLoadContext tests bit 30 (PPC numbering) of `state` - value 0x2,
// OS_CONTEXT_STATE_EXC - and takes the full `lmw r5, gpr[5]` restore path
// only when it is set, clearing it as it goes. Without it the restore is the
// setjmp-style partial one used by OSSaveContext's own return path, which
// would resume the interrupted code with most of its registers wrong.
constexpr u16 kContextStateException = 0x0002u;

// MSR[EE], PPC bit 16. An interrupt taken while the guest is inside
// OSDisableInterrupts would run a handler in what the guest believes is a
// critical section.
constexpr u32 kMsrExternalInterruptEnable = 0x00008000u;

// OS.c:13.  OSExceptionInit maps this physical address through
// OSPhysicalToCached(), so its guest-visible address is 0x80003000.  The
// table holds one handler pointer per OSException; OSAlarm installs its
// DecrementerExceptionHandler at index 8 (OS_EXCEPTION_DECREMENTER).
constexpr u32 kOsExceptionTable = 0x80003000u;
constexpr u32 kExceptionDecrementer = 8u;
constexpr u32 kDecrementerHandlerOffset = kExceptionDecrementer * 4u;

// DEC (SPR 22) decrements at the timebase rate.  OSAlarm's SetTimer passes
// its OSTime delta straight to PPCMtdec, which is direct evidence that the
// units are the same.  Keep it signed: hardware asserts the exception when
// the counter passes through zero and continues below it until reprogrammed.
s64 g_decrementer = 0;
bool g_decrementerArmed = false;
bool g_decrementerPending = false;
// Installing cpu->spr_read/spr_write takes over *every* SPR dr_cpu routes
// through them, not just DEC, so anything not handled here has to be given back
// exactly the behaviour it had when the hooks were null:
//
//   - SPR 287 (PVR) has its own case in ppc_mfspr and falls back to
//     PPC_GEKKO_PVR when there is no hook (extern/dolrecomp/src/cpu/cpu.c:488).
//     Absorbing it made the guest read a processor version of 0.
//   - Everything else reached `ppc_program_exception(cpu, PPC_PROGRAM_ILLEGAL,
//     cia)` (cpu.c:506), which step_game surfaces as a loud RunStop::Exception.
//
// An earlier version of this hook stored every SPR in a 1024-entry array and
// read it back, which silently swallowed HID0, WPAR, L2CR and GQR writes that
// used to be visible failures. Taking over a whole dispatch mechanism to
// implement one register of it is how a diagnostic goes quiet.
u32 decrementer_spr_read(CPUState *cpu, u16 spr, u32 cia)
{
    if (spr == 22) {
        return static_cast<u32>(g_decrementer);
    }
    if (spr == 287) {
        return PPC_GEKKO_PVR;
    }
    ppc_program_exception(cpu, PPC_PROGRAM_ILLEGAL, cia);
    return 0;
}

void decrementer_spr_write(CPUState *cpu, u16 spr, u32 value, u32 cia)
{
    if (spr != 22) {
        ppc_program_exception(cpu, PPC_PROGRAM_ILLEGAL, cia);
        return;
    }
    g_decrementer = static_cast<s32>(value);
    g_decrementerArmed = true;
    // A DEC write acknowledges the previous expiry and arms the next timer.
    // SetTimer() in OSAlarm does this both after a handled alarm and when a
    // nearer alarm is inserted, so this mirrors the SDK's actual protocol.
    g_decrementerPending = false;
}

void write_f64_bits(CPUState *cpu, u32 addr, f64 value)
{
    u64 bits;
    static_assert(sizeof(bits) == sizeof(value), "f64 is not 64 bits");
    std::memcpy(&bits, &value, sizeof(bits));
    mem_write32(cpu, addr, static_cast<u32>(bits >> 32));
    mem_write32(cpu, addr + 4, static_cast<u32>(bits));
}

// Writes the interrupted register file into the guest OSContext, doing by
// hand what the low-memory stub's OS_EXCEPTION_SAVE_GPRS plus OSSaveContext
// would have written.
//
// The floating-point registers are written for completeness, but note that
// OSLoadContext does NOT restore them - the SDK manages the FPU lazily
// through __OSFPUContext, and this port short-circuits that (see
// recomp_boot.cpp's FP-unavailable handling, which enables FP and resumes
// without swapping the FPU context). So floating-point state is effectively
// shared across whatever the handler does. That is a pre-existing gap this
// makes no worse and does not close.
void save_context(CPUState *cpu, u32 context)
{
    for (u32 i = 0; i < 32; ++i) {
        mem_write32(cpu, context + kContextGpr + i * 4, cpu->gpr[i]);
        write_f64_bits(cpu, context + kContextFpr + i * 8, cpu->fpr[i]);
        write_f64_bits(cpu, context + kContextPsf + i * 8, cpu->ps1[i]);
    }
    mem_write32(cpu, context + kContextCr, cpu->cr);
    mem_write32(cpu, context + kContextLr, cpu->lr);
    mem_write32(cpu, context + kContextCtr, cpu->ctr);
    mem_write32(cpu, context + kContextXer, cpu->xer);
    mem_write32(cpu, context + kContextFpscrPad, 0);
    mem_write32(cpu, context + kContextFpscr, cpu->fpscr);
    for (u32 i = 0; i < 8; ++i) {
        mem_write32(cpu, context + kContextGqr + i * 4, cpu->gqr[i]);
    }

    // srr0/srr1 are what OSLoadContext's closing rfi returns to: the
    // instruction that was about to run, and the MSR it was running under.
    mem_write32(cpu, context + kContextSrr0, cpu->pc);
    mem_write32(cpu, context + kContextSrr1, cpu->msr);

    mem_write16(cpu, context + kContextMode, 0);
    mem_write16(cpu, context + kContextState, kContextStateException);
}

#endif // DOLPHINJET_HAVE_RECOMPILED_GAME

} // namespace

void install()
{
    g_piShadow.fill(0);
    register_mmio_range({
        .base = kPiBase,
        .end = kPiEnd,
        .name = "PI",
        .read = &pi_read,
        .write = &pi_write,
    });

    g_viShadow.fill(0);
    register_mmio_range({
        .base = kViBase,
        .end = kViEnd,
        .name = "VI",
        .read = &vi_read,
        .write = &vi_write,
    });

    g_peShadow.fill(0);
    register_mmio_range({
        .base = kPeBase,
        .end = kPeEnd,
        .name = "PE",
        .read = &pe_read,
        .write = &pe_write,
    });
}

void raise(u32 causeBits)
{
    g_piShadow[kPiCauseOffset / 4] |= causeBits;
}

void clear(u32 causeBits)
{
    g_piShadow[kPiCauseOffset / 4] &= ~causeBits;
}

void raise_vi_retrace(CPUState *)
{
    g_viShadow[kViDisplayInterrupt0Offset / 4] |= kViInterruptBit;
    raise(kCauseVi);
}

void raise_pe_finish()
{
    // Set through the same insert()/extract() pair pe_read and pe_write use,
    // rather than by shifting a bit into the shadow by hand. __peReg[5] is the
    // halfword at byte offset 0x0A, which is the *low* half of the word at
    // 0x08 - getting that wrong puts the status bit in __peReg[4] instead,
    // which is a register this emulates but nothing acknowledges, so the finish
    // would never be cleared and would re-fire every frame.
    //
    // The guest clears this by writing the bit back through pe_write, so the
    // flag stays set until it does - which is what makes a second finish before
    // the first acknowledge visible instead of silently lost.
    insert(g_peShadow[kPeControlOffset / 4], kPeBase + kPeControlOffset, kPeControlFinishBit, 2);
    raise(kCausePeFinish);
}

u32 debug_cause()
{
    return g_piShadow[kPiCauseOffset / 4];
}

u32 debug_mask()
{
    return g_piShadow[kPiMaskOffset / 4];
}

bool run_pe_lane_self_test()
{
    // Start from a known-empty block and cause, the way install() leaves them.
    g_peShadow.fill(0);
    g_piShadow.fill(0);

    raise_pe_finish();

    if ((debug_cause() & kCausePeFinish) == 0) {
        Log.error("pe self-test: raise_pe_finish did not raise the PE_FINISH cause");
        return false;
    }

    // The whole point: read the status back the way the guest does, a halfword
    // at __peReg[5]. If the bit had landed in __peReg[4] - the other half of
    // the same 32-bit word - this read comes back zero and the guest's
    // GXFinishInterruptHandler would never see its own flag.
    const u32 control = extract(g_peShadow[kPeControlOffset / 4], kPeBase + kPeControlOffset, 2);
    if ((control & kPeControlFinishBit) == 0) {
        Log.error("pe self-test: the finish status bit is not visible at __peReg[5] (control={:#x})",
            control);
        return false;
    }
    // And the neighbouring register must NOT have it.
    const u32 neighbour = extract(g_peShadow[kPeControlOffset / 4], kPeBase + kPeControlOffset - 2, 2);
    if ((neighbour & kPeControlFinishBit) != 0) {
        Log.error("pe self-test: the finish status bit also landed in __peReg[4] ({:#x})", neighbour);
        return false;
    }

    // The token register is an ordinary shadowed halfword; a guest write has to
    // land and read back rather than being dropped.
    pe_write(nullptr, kPeBase + 0x0Eu, 0x1234u, 2);
    if (pe_read(nullptr, kPeBase + 0x0Eu, 2) != 0x1234u) {
        Log.error("pe self-test: the PE token register did not round-trip, got {:#x}",
            pe_read(nullptr, kPeBase + 0x0Eu, 2));
        return false;
    }

    // The acknowledge: GXFinishInterruptHandler writes its own status bit back,
    // and that write is what drops the cause.
    pe_write(nullptr, kPeBase + kPeControlOffset, kPeControlFinishBit, 2);
    if ((debug_cause() & kCausePeFinish) != 0) {
        Log.error("pe self-test: the guest's acknowledge did not drop the PE_FINISH cause");
        return false;
    }

    g_peShadow.fill(0);
    g_piShadow.fill(0);
    Log.info("pe self-test: the pixel-engine finish flag survives the block's halfword addressing "
             "and the guest's acknowledge clears its cause");
    return true;
}

bool pending(CPUState *)
{
    const u32 cause = g_piShadow[kPiCauseOffset / 4] & ~0x00010000u;
    const u32 mask = g_piShadow[kPiMaskOffset / 4];
    return cause != 0 && (cause & mask) != 0;
}

#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME

bool interrupts_enabled(CPUState *cpu) { return (cpu->msr & kMsrExternalInterruptEnable) != 0; }

bool dispatch(CPUState *cpu)
{
    if ((cpu->msr & kMsrExternalInterruptEnable) == 0) {
        return false;
    }
    const u32 context = mem_read32(cpu, kOSCurrentContextAddress);
    if (context < GC_RAM_BASE || context - GC_RAM_BASE >= cpu->ram_size) {
        // Before __OSThreadInit has run there is no context to save into, and
        // an out-of-range pointer is a real problem worth seeing rather than
        // writing 768 bytes over whatever it points at.
        static bool warned = false;
        if (!warned) {
            warned = true;
            Log.warn("dispatch: __OSCurrentContext is {:#010x}, not a guest RAM address - "
                     "no interrupt delivered until it is",
                context);
        }
        return false;
    }

    save_context(cpu, context);

    // Hardware clears MSR[EE] on taking the exception; the guest's own
    // OSLoadContext rfi restores the saved MSR, EE included, on the way out.
    cpu->msr &= ~kMsrExternalInterruptEnable;

    // What ExternalInterruptHandler (src/dolphin/os/OSInterrupt.c) does before
    // branching to __OSDispatchInterrupt: reserve a back-chain word on the
    // interrupted stack. Handlers run on the interrupted thread's stack, there
    // being no separate interrupt stack in this SDK.
    const u32 sp = cpu->gpr[1] - 8u;
    mem_write32(cpu, sp, cpu->gpr[1]);
    cpu->gpr[1] = sp;

    // r3 is the __OSException number. ExternalInterruptHandler marks it
    // `#pragma unused`, and __OSDispatchInterrupt never reads it, so the value
    // matters only for honesty: 0x500 / 0x100 is the external-interrupt
    // vector's index.
    cpu->gpr[3] = 5;
    cpu->gpr[4] = context;
    // One underscore after SYMBOL_, not two: the generator collapses the
    // leading pair (generated_symbols.h:24155).
    cpu->pc = DOLRECOMP_SYMBOL__OSDispatchInterrupt;

    // No return path on purpose: every path through __OSDispatchInterrupt ends
    // in OSLoadContext(context), so the guest resumes itself.
    return true;
}

bool advance_decrementer(CPUState *cpu, u64 elapsedTicks)
{
    if (!g_decrementerArmed || g_decrementerPending || elapsedTicks == 0) {
        return false;
    }

    const s64 elapsed = elapsedTicks > static_cast<u64>(INT64_MAX) ? INT64_MAX : static_cast<s64>(elapsedTicks);
    const s64 before = g_decrementer;
    g_decrementer = before <= INT64_MIN + elapsed ? INT64_MIN : before - elapsed;
    if (before > 0 && g_decrementer > 0) {
        return false;
    }

    const u32 context = mem_read32(cpu, kOSCurrentContextAddress);
    if (context < GC_RAM_BASE || context - GC_RAM_BASE >= cpu->ram_size) {
        // Before OSInit has established a current context there is no safe
        // place to save an asynchronous exception.  Leave DEC pending; the
        // first later boundary with a valid context will deliver it.
        return false;
    }

    const u32 handler = mem_read32(cpu, kOsExceptionTable + kDecrementerHandlerOffset);
    if (handler < GC_RAM_BASE || handler - GC_RAM_BASE >= cpu->ram_size) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            Log.warn("decrementer expired before OSInitAlarm installed a usable handler ({:#010x})", handler);
        }
        return false;
    }

    // The low-memory vector normally saves this state then looks up the
    // table entry above.  Those vectors are copied by OSExceptionInit at
    // runtime and are not present in a statically recompiled DOL, so perform
    // that tiny mechanical part here and execute the registered guest handler
    // itself.  DecrementerExceptionHandler saves the remaining volatile GPRs
    // before entering DecrementerExceptionCallback.
    save_context(cpu, context);
    cpu->msr &= ~kMsrExternalInterruptEnable;
    cpu->gpr[3] = kExceptionDecrementer;
    cpu->gpr[4] = context;
    cpu->pc = handler;
    g_decrementerPending = true;
    // Hard-capped, not throttled. Once the SDK's alarms actually run this fires
    // at their rate - tens to hundreds of times a second, indefinitely - and
    // this project has produced five runaway logs already. The first few
    // expiries are what answers "does DEC ever fire in a real run"; after that
    // the line says nothing new.
    static unsigned logged = 0;
    if (logged < 8) {
        ++logged;
        Log.info("decrementer: expired (remaining={}) -> handler={:#010x}{}", g_decrementer, handler,
            logged == 8 ? " [further expiries are not logged]" : "");
    }
    return true;
}

#else

// Without a recompiled game nothing is ever delivered, so nothing is ever
// takeable either. Matching dispatch() keeps the two answers consistent.
bool interrupts_enabled(CPUState *)
{
    return false;
}

bool dispatch(CPUState *)
{
    return false;
}

bool advance_decrementer(CPUState *, u64)
{
    return false;
}

#endif // DOLPHINJET_HAVE_RECOMPILED_GAME

void install_decrementer(CPUState *cpu)
{
#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME
    g_decrementer = 0;
    g_decrementerArmed = false;
    g_decrementerPending = false;
    cpu->spr_read = &decrementer_spr_read;
    cpu->spr_write = &decrementer_spr_write;
#else
    (void)cpu;
#endif
}

bool run_decrementer_self_test()
{
#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME
    CPUState cpu {};
    if (!cpu_init(&cpu)) {
        Log.error("decrementer self-test: cpu_init failed");
        return false;
    }

    install_decrementer(&cpu);
    constexpr u32 kContext = GC_RAM_BASE + 0x4000u;
    constexpr u32 kHandler = GC_RAM_BASE + 0x1000u;
    constexpr u32 kOriginalPc = GC_RAM_BASE + 0x2000u;
    constexpr u32 kOriginalMsr = kMsrExternalInterruptEnable | 0x00002000u;
    mem_write32(&cpu, kOSCurrentContextAddress, kContext);
    mem_write32(&cpu, kOsExceptionTable + kDecrementerHandlerOffset, kHandler);
    cpu.pc = kOriginalPc;
    cpu.msr = kOriginalMsr;

    // Route through dr_cpu's public SPR operation rather than calling our
    // callback directly: this catches a mismatch between the CPU access table
    // and the port hook as well as testing the countdown itself.
    ppc_mtspr(&cpu, 22, 10, cpu.pc);
    const bool firedEarly = advance_decrementer(&cpu, 9);
    const u32 remaining = ppc_mfspr(&cpu, 22, cpu.pc);
    const bool fired = advance_decrementer(&cpu, 1);

    const bool contextSaved = mem_read32(&cpu, kContext + kContextSrr0) == kOriginalPc &&
                              mem_read32(&cpu, kContext + kContextSrr1) == kOriginalMsr;
    const bool passed = !firedEarly && remaining == 1 && fired && cpu.pc == kHandler &&
                        cpu.gpr[3] == kExceptionDecrementer && cpu.gpr[4] == kContext &&
                        contextSaved && (cpu.msr & kMsrExternalInterruptEnable) == 0;
    cpu_free(&cpu);

    if (!passed) {
        Log.error("decrementer self-test: DEC did not preserve countdown/context or enter the registered handler");
        return false;
    }
    Log.info("decrementer self-test: DEC countdown and guest exception handoff work");
    return true;
#else
    Log.error("decrementer self-test requires generated game code");
    return false;
#endif
}

} // namespace sms::recomp::interrupt
