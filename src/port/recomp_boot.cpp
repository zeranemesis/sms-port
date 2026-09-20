#include "port/recomp_boot.h"

// generated/generated.h and generated/generated_symbols.h only exist once
// tools/port/recompile.py has run against a real GMSP01 dump (see
// CMakeLists.txt's DOLPHINJET_GENERATED_GAME_FILES glob) - guard this
// file's real content on the same macro CMakeLists.txt defines exactly
// when that's true, so a fresh checkout / CI without a dump still builds
// dolphinjet (menu shell only) against the stub below.
#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME

#include "port/recomp_card.h"
#include "port/recomp_dolphin_sdk.h"
#include "port/recomp_exi.h"
#include "port/recomp_gx_fifo.h"
#include "port/recomp_host.h"
#include "port/recomp_interrupt.h"
#include "port/recomp_pad.h"
#include "port/recomp_probe.h"

#include "aurora/lib/logging.hpp"

#include <dolphin/dvd.h>

// Like extern/dolrecomp's own cpu.h (see recomp_host.h), generated.h has no
// extern "C" guard of its own: every func_<address> it declares is plain C
// (chunk_*.c is compiled by the C compiler in the game_recompiled target),
// but including it unguarded from this .cpp file C++-mangles those
// declarations, so none of them match the plain-C symbols the linker
// actually has - verified as the real cause of a build's worth of
// "LNK2001: unresolved external symbol func_<address>" errors, one per
// chunk, the first time this file was written without this wrapper.
extern "C" {
#include "generated.h"
#include "generated_symbols.h"
}

#include <array>
#include <chrono>
#include <cstring>
#include <iterator>
#include <unordered_map>
#include <vector>

namespace sms::recomp {
namespace {

aurora::Module Log("sms::recomp::boot");

// dr_cpu's plain C backend implements the well-known architectural SPRs
// (LR, CTR, XER...) inline in generated code, and a few Gekko-specific
// ones natively in CPUState (hid2 - see cpu.h), but falls back to
// CPUState::instruction_fallback for others - verified empirically: the
// first real boot attempt against the GMSP01 dump hit "mfhid0 r3" at
// 0x8033b90c (early OS/cache-init code any GameCube game runs before
// touching game logic) and, with no fallback installed, that became an
// unhandled PPC_PROGRAM_ILLEGAL exception at vector 0x700 - there being no
// exception-vector code in the translated DOL for the CPU to jump to
// (real hardware's IPL would have installed a handler there; nothing here
// emulates the IPL boot sequence).
//
// HID0 and its kin (L2CR, WPAR, MMCR*, THRM*, ICTC...) configure hardware
// (caches, performance counters, thermal sensors) this host doesn't model
// and whose values the game never inspects for actual logic decisions -
// only real GameCube diagnostic/init code reads them back, typically just
// to OR in a documented bit and write it back. Treating every such SPR as
// a plain read/write-back storage cell (last value written, 0 initially)
// is the same stubbing real hardware-abstraction layers commonly do for
// this exact class of register - not a guess at what the register does,
// a decision to not emulate what it does because nothing here depends on
// it doing anything.
std::unordered_map<u32, u32> &unmodeled_spr_storage()
{
    static std::unordered_map<u32, u32> storage;
    return storage;
}

// --- Gekko locked cache -------------------------------------------------------
//
// The Gekko can lock half of its L1 data cache and map it at 0xE0000000 as a
// 16KB scratchpad (LC_BASE, include/dolphin/os/OSCache.h:19-21). It is not
// backed by main memory: data is moved in and out explicitly by a small DMA
// engine programmed through two SPRs.
//
// The THP video decoder depends on both. It refuses to run at all unless the
// locked cache is enabled - `if (!(PPCMfhid2() & 0x10000000)) goto
// _err_lc_not_enabled;` (src/dolphin/thp/THPDec.c:49) - does its IDCT into LC
// scratch buffers, and then moves each decoded macroblock row out with
// LCStoreData -> LCStoreBlocks (src/dolphin/os/OSCache.c).
//
// Neither existed here. Writes to 0xE0000000 fell through to the unmapped-MMIO
// path and were dropped, and `mtspr 922/923` landed in the SPR storage below
// and did nothing. So the decoder wrote its output into the void and the DMA
// that should have rescued it was a no-op - measured as decoded Y/U/V planes
// that were *entirely* zero (nonZero=0 over all 286,720 bytes) while
// THPVideoDecode reported success and the player's frame counters advanced.
constexpr u32 kLockedCacheBase = 0xE0000000u;
constexpr u32 kLockedCacheSize = 16u * 1024u;
std::array<u8, kLockedCacheSize> g_lockedCache {};

// `OSLoadContext` deliberately turns MSR[FP] off.  On hardware the following
// FP-unavailable exception enters the SDK's lazy-FPU handler, which saves the
// previous owner and restores the current OSContext.  The static recompilation
// has no translated exception vector at 0x800, so this port must reproduce the
// handler's observable state transition before resuming the faulting opcode.
//
// Without it every GameCube thread shares CPUState::fpr/ps1.  This is
// especially visible in THP: its decoder runs in a dedicated thread and is
// preempted between IDCT operations, turning otherwise-valid coefficients into
// noisy image bands when another thread borrows the FPU.
// RunQueueBits = .sbss:0x80405ED8 (config/GMSP01/symbols.txt:30995) and
// IdleContext = .bss:0x803FA558, size 0x2C8 - which matches sizeof(OSContext)
// exactly, a useful cross-check that it is the object it claims to be.
constexpr u32 kRunQueueBitsAddress = 0x80405ED8u;
constexpr u32 kIdleContextAddress = 0x803FA558u;
constexpr u32 kOsCurrentContextAddress = GC_RAM_BASE + 0xD4u;
constexpr u32 kOsFpuContextAddress = GC_RAM_BASE + 0xD8u;
constexpr u32 kOsContextFprOffset = 0x90u;
constexpr u32 kOsContextFpscrOffset = 0x190u;
constexpr u32 kOsContextStateOffset = 0x1A2u;
constexpr u32 kOsContextPsfOffset = 0x1C8u;
constexpr u32 kOsContextSize = 0x2C8u;
constexpr u16 kOsContextStateFpSaved = 0x01u;
constexpr u64 kMffsPayloadPrefix = 0xFFF8000000000000ull;

bool is_guest_context(CPUState *cpu, u32 address)
{
    return address >= GC_RAM_BASE
        && static_cast<u64>(address - GC_RAM_BASE) + kOsContextSize <= cpu->ram_size;
}

void save_fpu_context(CPUState *cpu, u32 context)
{
    u16 state = mem_read16(cpu, context + kOsContextStateOffset);
    mem_write16(cpu, context + kOsContextStateOffset, state | kOsContextStateFpSaved);
    for (u32 index = 0; index < 32; ++index) {
        mem_write64(cpu, context + kOsContextFprOffset + index * 8u,
            dolrecomp_f64_to_bits(cpu->fpr[index]));
        mem_write64(cpu, context + kOsContextPsfOffset + index * 8u,
            dolrecomp_f64_to_bits(cpu->ps1[index]));
    }
    mem_write64(cpu, context + kOsContextFpscrOffset, kMffsPayloadPrefix | cpu->fpscr);
}

void load_fpu_context(CPUState *cpu, u32 context)
{
    if ((mem_read16(cpu, context + kOsContextStateOffset) & kOsContextStateFpSaved) == 0) {
        return;
    }
    for (u32 index = 0; index < 32; ++index) {
        cpu->fpr[index] = dolrecomp_f64_from_bits(mem_read64(cpu, context + kOsContextFprOffset + index * 8u));
        cpu->ps1[index] = dolrecomp_f64_from_bits(mem_read64(cpu, context + kOsContextPsfOffset + index * 8u));
    }
    cpu->fpscr = static_cast<u32>(mem_read64(cpu, context + kOsContextFpscrOffset));
}

bool handle_fp_unavailable(CPUState *cpu)
{
    const u32 current = mem_read32(cpu, kOsCurrentContextAddress);
    const u32 owner = mem_read32(cpu, kOsFpuContextAddress);
    if (!is_guest_context(cpu, current)) {
        // FP can be used during the early boot sequence, before OSInit has
        // published a context. The old behavior is the only valid fallback.
        return false;
    }
    if (owner != current && is_guest_context(cpu, owner)) {
        save_fpu_context(cpu, owner);
    }
    if (owner != current) {
        load_fpu_context(cpu, current);
        mem_write32(cpu, kOsFpuContextAddress, current);
    }
    return true;
}

// Writes the guest has made to each 32-byte line since that line was last
// carried out by a store DMA. This is what tells a locked cache full of noise
// that the decoder *put* there apart from one the decoder never wrote at all,
// and those two have completely different causes.
constexpr size_t kLockedCacheLines = kLockedCacheSize / 32u;
std::array<u32, kLockedCacheLines> g_lcWritesSinceStore {};

bool locked_cache_offset(u32 addr, u8 size, u32 *offsetOut)
{
    if (addr < kLockedCacheBase) {
        return false;
    }
    const u32 offset = addr - kLockedCacheBase;
    if (u64(offset) + size > kLockedCacheSize) {
        return false;
    }
    *offsetOut = offset;
    return true;
}

// Big-endian, like guest RAM: this is ordinary storage the guest addresses
// with ordinary loads and stores, not a register block.
u64 locked_cache_read(CPUState *, u32 addr, u8 size)
{
    u32 offset = 0;
    if (!locked_cache_offset(addr, size, &offset)) {
        return 0;
    }
    u64 value = 0;
    for (u8 i = 0; i < size; ++i) {
        value = (value << 8) | g_lockedCache[offset + i];
    }
    return value;
}

void locked_cache_write(CPUState *, u32 addr, u64 value, u8 size)
{
    u32 offset = 0;
    if (!locked_cache_offset(addr, size, &offset)) {
        return;
    }
    for (u8 i = 0; i < size; ++i) {
        g_lockedCache[offset + (size - 1 - i)] = static_cast<u8>(value >> (i * 8));
    }
    // Count both ends: a store of up to 8 bytes can straddle two lines.
    ++g_lcWritesSinceStore[offset / 32u];
    ++g_lcWritesSinceStore[(offset + size - 1u) / 32u];
}

// DMA_U (SPR 922) and DMA_L (SPR 923). The encoding is read straight out of
// LCStoreBlocks/LCLoadBlocks (src/dolphin/os/OSCache.c), which are the only
// writers:
//
//     DMA_U = (mainAddress & 0x0FFFFFFF) | (numBlocks >> 2)
//     DMA_L = (lcAddress & ~0x1F) | ((numBlocks & 3) << 2) | trigger | direction
//
// so the 7-bit block count is split across the two registers' low bits, which
// the 32-byte alignment of both addresses leaves free. The trigger is bit 1,
// and bit 4 selects the direction: LCLoadBlocks ORs in 0x12 (load, main -> LC)
// where LCStoreBlocks ORs in 0x2 (store, LC -> main). A count of 0 means 128,
// which is how LCStoreData asks for a full 4KB transaction.
constexpr u32 kSprDmaUpper = 922;
constexpr u32 kSprDmaLower = 923;
constexpr u32 kDmaTriggerBit = 0x2u;
constexpr u32 kDmaLoadBit = 0x10u;
u32 g_dmaUpper = 0;

// Recent store DMAs (locked cache -> main memory), newest last. Sized to hold
// well over one decoded THP frame: the decoder issues five LCStoreBlocks per
// macroblock row (LCStoreData splits a 320-block Y row into 128+128+64, plus
// one each for U and V) and a 640x448 frame is 28 macroblock rows, so ~140
// entries per frame. 2048 covers roughly fourteen frames, which is more than
// the triple-buffered texture sets can be behind.
struct LockedCacheStore {
    u32 guestBase;
    u32 bytes;
};
constexpr size_t kLockedCacheStoreRing = 2048;
std::array<LockedCacheStore, kLockedCacheStoreRing> g_lcStores {};
size_t g_lcStoreNext = 0;
u64 g_lcStoreTotal = 0;

void record_locked_cache_store(u32 guestBase, u32 bytes)
{
    g_lcStores[g_lcStoreNext] = LockedCacheStore { guestBase, bytes };
    g_lcStoreNext = (g_lcStoreNext + 1) % kLockedCacheStoreRing;
    ++g_lcStoreTotal;
}

void perform_locked_cache_dma(CPUState *cpu, u32 lower)
{
    const u32 mainOffset = g_dmaUpper & 0x0FFFFFE0u;
    const u32 lcAddress = lower & 0xFFFFFFE0u;
    u32 blocks = ((g_dmaUpper & 0x1Fu) << 2) | ((lower >> 2) & 0x3u);
    if (blocks == 0) {
        blocks = 128;
    }
    const u32 bytes = blocks * 32u;
    const bool load = (lower & kDmaLoadBit) != 0;

    u32 lcOffset = 0;
    if (!locked_cache_offset(lcAddress, 1, &lcOffset) || u64(lcOffset) + bytes > kLockedCacheSize
        || u64(mainOffset) + bytes > cpu->ram_size) {
        static unsigned warned = 0;
        if (warned < 8) {
            ++warned;
            Log.warn("locked-cache DMA out of range: lc={:#010x} main={:#010x} bytes={:#x} {}", lcAddress, mainOffset,
                bytes, load ? "load" : "store");
        }
        return;
    }

    // Byte-wise both ways: this is opaque payload, not words, and must not be
    // byte-swapped in either direction.
    const u32 guestBase = mainOffset | GC_RAM_BASE;
    for (u32 i = 0; i < bytes; ++i) {
        if (load) {
            g_lockedCache[lcOffset + i] = mem_read8(cpu, guestBase + i);
        } else {
            mem_write8(cpu, guestBase + i, g_lockedCache[lcOffset + i]);
        }
    }

    if (!load) {
        record_locked_cache_store(guestBase, bytes);

        // The bands are already noise in the plane in guest RAM, and the store
        // that delivered the noisy row did happen (measured: covered=true for
        // the noisy row and for a control row of real picture alike). So the
        // transfer is not dropping anything - either the locked cache already
        // held noise, or this copy corrupts it. Measuring the SOURCE separates
        // those two, and nothing else can.
        //
        // Same roughness statistic as the plane probe: mean |b[i+1]-b[i]| over
        // horizontal neighbours inside each 8-byte tile run. Real picture sits
        // in the single digits, uniform random bytes average 85.3.
        u64 diffSum = 0;
        u32 diffCount = 0;
        for (u32 i = 0; i + 1 < bytes; ++i) {
            if ((i & 7u) == 7u) {
                continue;
            }
            const int a = g_lockedCache[lcOffset + i];
            const int b = g_lockedCache[lcOffset + i + 1];
            diffSum += u32(b > a ? b - a : a - b);
            ++diffCount;
        }
        const double roughness = diffCount == 0 ? 0.0 : double(diffSum) / double(diffCount);
        // Rate-limited to a handful per second, so it cannot become the sixth
        // runaway log. 60 is far above any real frame and far below noise.
        static u64 lastReport = 0;
        static unsigned reportsThisSecond = 0;
        if (cpu->timebase - lastReport >= 40500000ull) {
            lastReport = cpu->timebase;
            reportsThisSecond = 0;
        }
        // Which of the lines about to leave were actually written by the
        // guest since they last left? If none were, the decoder's output never
        // reached this scratchpad and we are shipping stale bytes; if all were,
        // the decoder itself computed noise. Nothing else distinguishes those.
        u32 unwrittenLines = 0;
        const u32 firstLine = lcOffset / 32u;
        const u32 lineCount = bytes / 32u;
        for (u32 line = firstLine; line < firstLine + lineCount && line < kLockedCacheLines; ++line) {
            if (g_lcWritesSinceStore[line] == 0) {
                ++unwrittenLines;
            }
        }
        if (roughness > 60.0 && reportsThisSecond < 4) {
            ++reportsThisSecond;
            Log.warn("locked-cache store carries noise: lc={:#010x} main={:#010x} bytes={:#x} roughness={:.1f} "
                     "unwrittenLines={}/{} pc={:#010x}",
                lcAddress, guestBase, bytes, roughness, unwrittenLines, lineCount, cpu->pc);
        }
        for (u32 line = firstLine; line < firstLine + lineCount && line < kLockedCacheLines; ++line) {
            g_lcWritesSinceStore[line] = 0;
        }
    }

    static unsigned logged = 0;
    if (logged < 4) {
        ++logged;
        Log.info("locked-cache DMA {}: lc={:#010x} main={:#010x} bytes={:#x}{}", load ? "load" : "store", lcAddress,
            guestBase, bytes, logged == 4 ? " [further transfers are not logged]" : "");
    }
}

// Decodes only what's needed to tell mfspr/mtspr apart from every other
// instruction that might reach this fallback (see the PowerPC ISA's
// XFX-form encoding: primary opcode 31, spr split across bits 11-20 with
// its two 5-bit halves swapped, secondary opcode in bits 21-30) - verified
// by hand-decoding 0x7C70FAA6 ("mfhid0 r3" per dolrecomp's own decoder
// comment) to primary=31, rD=3, spr=1008 (HID0), secondary=339 (mfspr),
// matching the documented encoding exactly.
void handle_instruction_fallback(CPUState *cpu, u32 raw, u32 cia)
{
    const u32 primaryOp = raw >> 26;
    const u32 rD_rS = (raw >> 21) & 0x1Fu; // bits 6-10 (X-form rD/rS, XFX-form rD/rS)
    const u32 rA = (raw >> 16) & 0x1Fu;    // bits 11-15 (X-form rA; XFX-form spr low 5 bits)
    const u32 rB = (raw >> 11) & 0x1Fu;    // bits 16-20 (X-form rB; XFX-form spr high 5 bits)
    const u32 secondaryOp = (raw >> 1) & 0x3FFu; // bits 21-30

    // Every path below emulates exactly one guest instruction, so charge one
    // instruction's worth of the slice budget. Without this the fallback is
    // another callee that advances pc while spending nothing, which is what
    // stops a slice from ever ending on cycles (see recomp_host.cpp's
    // kBridgedCallCycles).
    cpu->downcount -= 1;

    if (primaryOp == 31 && secondaryOp == 339) { // mfspr
        // XFX-form's spr field packs its two 5-bit halves in reverse order
        // (bits 11-15 = low 5 bits, bits 16-20 = high 5 bits) - rA/rB above
        // are exactly those two fields, already split out.
        const u32 spr = (rB << 5) | rA;
        cpu->gpr[rD_rS] = unmodeled_spr_storage()[spr];
        cpu->pc = cia + 4;
        return;
    }
    if (primaryOp == 31 && secondaryOp == 467) { // mtspr
        const u32 spr = (rB << 5) | rA;
        const u32 value = cpu->gpr[rD_rS];
        unmodeled_spr_storage()[spr] = value;
        // The locked-cache DMA is the one SPR pair here that has to *do*
        // something rather than just be remembered. DMA_L's trigger bit is
        // written last, which is what starts the transfer.
        if (spr == kSprDmaUpper) {
            g_dmaUpper = value;
        } else if (spr == kSprDmaLower && (value & kDmaTriggerBit) != 0) {
            perform_locked_cache_dma(cpu, value);
        }
        cpu->pc = cia + 4;
        return;
    }

    // Cache-management hints (dcbf/dcbst/dcbi/dcbt/dcbtst/icbi): no cache is
    // modeled here, so there is nothing for them to do - verified against
    // dr_cpu's own ppc_cache_control() (extern/dolrecomp/src/cpu/cpu.c),
    // whose default behavior with no cpu->cache_control installed is
    // exactly a no-op for all of these (bar dcbi in user mode, a privilege
    // exception dolphinjet's supervisor-mode CPUState never triggers).
    // dcbz is different and NOT safe to no-op: real GameCube code uses it
    // to fast-zero 32-byte-aligned buffers, an effect games can observe.
    if (primaryOp == 31 &&
        (secondaryOp == 86 ||  // dcbf
         secondaryOp == 54 ||  // dcbst
         secondaryOp == 470 || // dcbi
         secondaryOp == 278 || // dcbt
         secondaryOp == 246 || // dcbtst
         secondaryOp == 982    // icbi
        )) {
        cpu->pc = cia + 4;
        return;
    }
    if (primaryOp == 31 && secondaryOp == 1014) { // dcbz
        const u32 ea = ((rA != 0 ? cpu->gpr[rA] : 0u) + cpu->gpr[rB]) & ~0x1Fu;
        for (u32 offset = 0; offset < 32; offset += 4) {
            mem_write32(cpu, ea + offset, 0);
        }
        cpu->pc = cia + 4;
        return;
    }

    Log.error("instruction_fallback: unhandled raw={:#010x} at pc={:#010x} (primary={} secondary={})", raw, cia, primaryOp, secondaryOp);
    ppc_program_exception(cpu, PPC_PROGRAM_ILLEGAL, cia);
}

// The `sc` (system call) instruction always raises PPC_EXC_SYSTEM_CALL,
// architecturally - real hardware has no default handler of its own; the
// IPL/BS2 bootrom installs a minimal one at the vector (0xC00) before ever
// jumping to a game, matching this port's dr_cpu-based CPUState having no
// such vector translated either (DolRecomp translated the DOL, not the
// IPL). Verified hitting this for real, inside DCFlushRange
// (generated_symbols.h: DOLRECOMP_SYMBOL_DCFlushRange 0x8033B80C, size
// 0x34, our fault address 0x8033b83c falls inside that range) - a GameCube
// SDK cache-flush routine, not application code expecting a real syscall
// ABI, consistent with `sc` here being the documented "debugger trap
// point" convention: harmless to skip when nothing is attached to catch
// it. `ppc_rfi` (extern/dolrecomp/src/cpu/cpu.c) restores msr from srr1 and
// sets pc = srr0 exactly, so resuming is just a matter of srr0 already being
// right - and it is: ppc_system_call_exception() (same file) raises the
// exception with `cia + 4u`, which is what the PowerPC architecture
// specifies for System Call (SRR0 <- CIA+4, unlike traps that leave SRR0 on
// the faulting instruction).
//
// This used to add another 4 on top of that, on the stated premise that
// srr0 pointed AT the `sc`. It does not, and the extra bump skipped exactly
// one instruction after every single `sc` in the game. That is invisible
// most of the time and fatal in one specific place: PPCSync() is `sc; blr`
// at 0x80339C38, and PPCHalt - an unconditional infinite loop - starts four
// bytes later at 0x80339C40. Skipping PPCSync's `blr` therefore fell
// straight into PPCHalt, which is exactly where boot was parked (measured:
// pc=0x80339C44 inside loop_80339C44, lr pointing back into __ARChecksize,
// which contains no PPCHalt call of its own).
bool handle_system_call(CPUState *cpu, u32 /*address*/)
{
    ppc_rfi(cpu, cpu->pc);
    return true;
}

// Why a translated block run stopped. dolrecomp_run_blocks only reports
// "finished the budget" or "stopped"; the caller always has to know which kind
// of stop it was, and both run loops in this file resolve the same two
// exceptions the same way, so they share this.
enum class RunStop {
    BudgetExhausted,     // ran maxBlocks blocks without stopping
    Halted,              // stopped with no exception - inspect cpu->pc
    Exception,           // stopped on an exception this port does not handle
    RepeatedSystemCalls, // the sc retry cap tripped
};

// dolrecomp_run_blocks (generated.h:621) counts blocks and never looks at
// downcount, so a "slice" ended after N blocks regardless of how much guest
// time those blocks represented. Block length varies enormously - a tight loop
// the recompiler extracted into one block burns thousands of cycles, a run of
// straight-line code burns a handful - which makes the block count a poor proxy
// for time, and the measurement said exactly that:
//
//   frame budget: guest time advanced 620008 of 675000 ticks (92%) | 64/64 slices hit the block budget
//   frame budget: guest time advanced  45901 of 675000 ticks ( 7%) | 64/64 slices hit the block budget
//
// Every slice of every frame ran out of blocks, and a frame bought anywhere
// between 7% and 92% of a real GameCube frame. The guest was permanently cut
// off mid-frame and living in slow motion, which is what the THP decoder
// running at half its declared 29.97fps looks like from the inside.
//
// So run until the *cycle* budget is spent, which is what models a frame, and
// demote the block count to the safety cap it was always described as. This
// also gives the idle case the right behaviour for free: a guest with nothing
// to run spins in SelectThread's `while (RunQueueBits == 0)` loop
// (OSThread.c) and burns the rest of its frame there, exactly as the hardware
// does, instead of the frame ending at an arbitrary block.
//
// Returns true when a budget ran out (the caller's "keep going" case), false
// when dolrecomp_call declined a block or an exception is pending - the same
// contract dolrecomp_run_blocks had.
// Set by run_blocks_until_budget_spent so the frame probe can say WHICH budget
// ended the slice. Counting "a budget ran out" answers nothing now that either
// one can do it, and reading that number as if it still meant blocks would be
// the same mistake as every other probe that was coarser than its subject.
bool g_lastSliceHitBlockCap = false;

// How many blocks a frame actually executes. Cycles per block is what says
// whether the per-block dispatch cost matters: dolrecomp_call does a host-call
// hash lookup on every single block (generated.h's dolrecomp_call, and
// recomp_host.cpp's dispatch() which is an unordered_map::find), so if blocks
// are short that lookup is a large fraction of the work.
u64 g_blocksThisFrame = 0;

bool run_blocks_until_budget_spent(CPUState *cpu, unsigned maxBlocks)
{
    g_lastSliceHitBlockCap = false;
    for (unsigned blocks = 0; maxBlocks == 0u || blocks < maxBlocks; ++blocks) {
        if (cpu->downcount <= 0) {
            return true;
        }
        // Removing the block cap stalled the game outright, which means some
        // callee returns without spending any of the slice's cycles: the slice
        // then never ends on downcount, and interrupt delivery - which only
        // happens between slices - starves. Naming that callee is what lets the
        // cap go back to being a safety net instead of the thing holding the
        // frame together, so record it rather than leave it as "something".
        const u32 calledPc = cpu->pc;
        const s64 downcountBefore = cpu->downcount;
        ++g_blocksThisFrame;
        if (!dolrecomp_call(cpu, cpu->pc)) {
            return false;
        }
        if (cpu->exception) {
            return false;
        }
        if (cpu->downcount == downcountBefore) {
            // Bounded the way every probe in this port is bounded: one line per
            // distinct address AND a hard cap, because a free-running caller is
            // exactly the shape that has produced five runaway logs here.
            static std::unordered_map<u32, u64> freeBlocks;
            static bool capReported = false;
            auto [it, inserted] = freeBlocks.try_emplace(calledPc, 0);
            ++it->second;
            if (inserted) {
                if (freeBlocks.size() <= 16) {
                    Log.warn("block at {:#010x} returned without spending any cycles - a slice made only of these "
                             "never ends on its cycle budget",
                        calledPc);
                } else if (!capReported) {
                    capReported = true;
                    Log.warn("more than 16 distinct addresses return without spending cycles; no further ones are "
                             "logged");
                }
            }
        }
    }
    g_lastSliceHitBlockCap = true;
    return true;
}

RunStop run_blocks(CPUState *cpu, unsigned maxBlocks)
{
    // dolrecomp_run_blocks (generated.h) stops as soon as CPUState::exception
    // is non-zero, *before* dispatching to the vector address it just set
    // cpu->pc to - verified: registering a host-call handler for
    // PPC_VECTOR_SYSTEM_CALL through recomp_host.h's normal table never
    // fired, because the loop bails out on ctx->exception first, so
    // dolrecomp_call(ctx, 0xC00) - where that handler would have been
    // looked up - is never reached. Handling PPC_EXC_SYSTEM_CALL here
    // instead, directly, then clearing the flag and re-running
    // dolrecomp_run_blocks for the same budget, is what actually reaches
    // handle_system_call. Capped retry count, not the block budget itself,
    // guards against a hypothetical sc-in-a-tight-loop from stalling a
    // frame - each retry already ran up to maxBlocks real blocks first.
    for (int retry = 0; retry < 64; ++retry) {
        if (run_blocks_until_budget_spent(cpu, maxBlocks)) {
            return RunStop::BudgetExhausted;
        }
        // The SDK runs with MSR[FP] deliberately off and switches the FPU in
        // on demand: the first floating-point instruction a thread executes
        // traps to the 0x800 vector, whose handler enables FP (swapping the
        // FPU context in on the way) and returns to the faulting
        // instruction. That handler is copied into low memory by
        // __OSExceptionInit at runtime, so a *static* recompiler never
        // translated anything at 0x800 - measured: "CPU exception 0x20 ... at
        // srr0=0x80011e94, vector pc=0x00000800", two step_game() calls in,
        // and the guest dead from there. Setting MSR[FP] at boot does not
        // help either (verified: the boot log shows msr=0x00002000 and the
        // same trap still fires) precisely because the guest clears it again
        // itself.
        //
        // Reproduce the handler's lazy ownership transfer before enabling FP
        // and resuming at the faulting instruction. This keeps each guest
        // thread's FPR/paired-single state isolated even though the vector
        // itself is not part of the statically recompiled DOL.
        if (cpu->exception == PPC_EXC_FP_UNAVAILABLE) {
            cpu->exception = 0;
            handle_fp_unavailable(cpu);
            // Return the way the real handler does - through rfi, which
            // restores BOTH pc (from srr0) and msr (from srr1).
            //
            // This used to do `cpu->msr |= MSR_FP; cpu->pc = cpu->srr0;`, which
            // gets the pc right and the msr catastrophically wrong: taking the
            // exception already ran exception_msr() (extern/dolrecomp/src/cpu/
            // cpu.c), which clears EE, IR, DR and RI, and resuming without an
            // rfi left them cleared forever. So the first floating-point
            // instruction a thread ever executed permanently disabled its
            // interrupts.
            //
            // Measured, once interrupt delivery existed to notice it: MSR[EE]
            // was clear at 172,801 out of 172,801 sampled slice boundaries,
            // with msr=0x00003000 - exactly FP|ME, an exception-entry MSR with
            // FP added back. A backtrace put the guest in JAudio's audio thread
            // (audioproc -> Driver::init -> DSPInterface::initBuffer ->
            // DsetupTable -> DSPSendCommands2), spinning on a flag only an
            // interrupt could ever set.
            ppc_rfi(cpu, cpu->pc);
            cpu->msr |= 0x00002000u; // MSR[FP], PPC bit 18 - the handler's actual job
            continue;
        }
        if (cpu->exception == PPC_EXC_SYSTEM_CALL) {
            cpu->exception = 0;
            handle_system_call(cpu, cpu->pc);
            continue;
        }
        return cpu->exception ? RunStop::Exception : RunStop::Halted;
    }
    return RunStop::RepeatedSystemCalls;
}

// --- interrupt delivery -----------------------------------------------------
//
// This replaces a narrower mechanism that called __VIRetraceHandler directly,
// but only while the guest sat in SelectThread's idle loop, where no thread
// context could be lost. That got the scheduler moving and then hit its own
// limit immediately: the next stall was a busy-wait in game code
// (DSPSendCommands2's `while (Dsp_Running_Check() == 0) ;`), which is not a
// scheduler idle point and never will be, so there was no safe moment to
// special-case. Interrupts have to be deliverable wherever the guest is.
//
// recomp_interrupt.cpp does that properly - see its header for the whole
// argument. All that is left here is deciding *when*.
//
// An interrupt can only be taken where the host holds control, i.e. between
// runs of translated blocks, so how often the host takes control back is
// exactly the interrupt latency. Running a whole frame in one go made that
// latency a full frame, which is not what any of this hardware looks like:
// VI fires once a frame, but DSP, AI, DVD and EXI fire whenever they are
// ready. The frame is therefore run in slices (see step_game), and this is
// tried at every slice boundary.
// Walks the guest's stack back-chain and logs the return address saved in each
// frame. The PowerPC EABI the SDK is built for stores the caller's stack
// pointer at [r1] and the return address at [r1 + 4], so this is the same walk
// a debugger would do.
//
// It exists because a pc alone does not say how the guest got somewhere, and
// "stuck in a loop with interrupts disabled" is a question about the caller,
// not about the loop.
void log_guest_backtrace(CPUState *cpu, unsigned maxFrames)
{
    Log.info("backtrace: pc={:#010x} lr={:#010x} r1={:#010x} msr={:#010x}", cpu->pc, cpu->lr, cpu->gpr[1], cpu->msr);
    u32 sp = cpu->gpr[1];
    for (unsigned frame = 0; frame < maxFrames; ++frame) {
        if (sp < GC_RAM_BASE || sp - GC_RAM_BASE + 8 > cpu->ram_size || (sp & 3u) != 0) {
            Log.info("backtrace:   [{}] stack pointer {:#010x} is not usable, stopping", frame, sp);
            return;
        }
        const u32 next = mem_read32(cpu, sp);
        const u32 returnAddress = mem_read32(cpu, sp + 4);
        Log.info("backtrace:   [{}] sp={:#010x} lr={:#010x}", frame, sp, returnAddress);
        if (next <= sp) {
            // The chain must climb; anything else is the end of it (or garbage
            // worth not following).
            return;
        }
        sp = next;
    }
}

// Returns true when the guest was actually pushed into
// __OSDispatchInterrupt, which the caller needs to know: a slice that
// just delivered an interrupt has a handler to run and must not be
// skipped, however idle the guest looked a moment earlier.
bool deliver_pending_interrupts(CPUState *cpu)
{
    // Bounded outcome trace: whether an interrupt is actually being taken is
    // not observable from the guest's pc alone, and "it is stuck in the same
    // place" is equally consistent with dispatch working and with dispatch
    // never firing. One line per distinct outcome, plus a count, says which.
    static unsigned dispatched = 0;
    static unsigned skippedMasked = 0;
    static unsigned skippedNotTakeable = 0;
    // Periodic rather than first-N: the first frames are all boot, and the
    // question is what the steady state looks like. Every 300 calls is about
    // once every five seconds at 60Hz, capped so it cannot run away.
    static unsigned calls = 0;
    static unsigned traced = 0;
    ++calls;
    const bool traceThisCall = (calls % 19200u) == 1u && traced < 24;
    const auto trace = [&](const char *outcome) {
        if (!traceThisCall) {
            return false;
        }
        ++traced;
        Log.info("interrupt: {} after {} frames (dispatched={} masked={} not-takeable={}) "
                 "msr={:#010x} piCause={:#010x} piMask={:#010x} pc={:#010x}",
            outcome, calls, dispatched, skippedMasked, skippedNotTakeable, cpu->msr, interrupt::debug_cause(),
            interrupt::debug_mask(), cpu->pc);
    };

    if (!interrupt::pending(cpu)) {
        ++skippedMasked;
        trace("masked");
        // The guest has not unmasked PI_VI yet (SetInterruptMask,
        // src/dolphin/os/OSInterrupt.c:244, is what ORs 0x100 into PI's mask).
        // Nothing to do but leave the cause standing until it does.
        return false;
    }
    if (!interrupt::dispatch(cpu)) {
        ++skippedNotTakeable;
        trace("not takeable");
        // One backtrace, the first time this is reached in the steady state:
        // MSR[EE] being clear here means some caller disabled interrupts and
        // never restored them, and only the call chain says which.
        static bool backtraced = false;
        if (!backtraced && skippedNotTakeable > 4096) {
            backtraced = true;
            log_guest_backtrace(cpu, 16);
        }
        // MSR[EE] clear, or no context to save into yet. The cause stays
        // pending, so the next frame tries again - which is what a real level-
        // triggered interrupt line does, rather than being dropped.
        return false;
    }
    ++dispatched;
    trace("dispatched");
    // Control does not come back from dispatch(): the guest is now inside
    // __OSDispatchInterrupt and will resume itself through OSLoadContext. The
    // cycle budget it spends there is its own.
    rebase_guest_timebase(cpu);
    return true;
}

// GameCube DOL header: 7 text + 11 data sections, all big-endian u32 at
// fixed offsets - verified against extern/dolrecomp's own parser
// (extern/dolrecomp/src/frontend/container/dol.c, dol_load()). That parser
// reads from a file path via fopen, but DVDGetDOLLocation hands back the
// raw bytes nod already read off the mounted disc image, not a path - so
// this reads the same fixed layout directly out of that in-memory buffer
// instead of writing it to a temp file just to reuse dol_load().
u32 read_be32(const u8 *base, u32 offset)
{
    return (u32(base[offset]) << 24) | (u32(base[offset + 1]) << 16) |
           (u32(base[offset + 2]) << 8) | u32(base[offset + 3]);
}

bool load_dol_section(CPUState *cpu, const u8 *dol, u32 dolSize, const char *label, int index,
    u32 offset, u32 address, u32 size)
{
    if (size == 0) {
        return true;
    }
    if (u64(offset) + size > dolSize) {
        Log.error("DOL {} section {} (offset={:#x} size={:#x}) runs past the {}-byte DOL", label, index, offset, size, dolSize);
        return false;
    }
    if (address < GC_RAM_BASE || u64(address - GC_RAM_BASE) + size > cpu->ram_size) {
        Log.error("DOL {} section {} (address={:#x} size={:#x}) falls outside the {}MB guest RAM", label, index, address, size, cpu->ram_size / (1024 * 1024));
        return false;
    }
    std::memcpy(cpu->ram + (address - GC_RAM_BASE), dol + offset, size);
    return true;
}

bool load_dol_into_ram(CPUState *cpu, const u8 *dol, u32 dolSize)
{
    if (dolSize < 0x100) {
        Log.error("DOL too small to hold a header ({} bytes)", dolSize);
        return false;
    }

    for (int i = 0; i < 7; ++i) {
        const u32 offset = read_be32(dol, 0x00 + u32(i) * 4);
        const u32 address = read_be32(dol, 0x48 + u32(i) * 4);
        const u32 size = read_be32(dol, 0x90 + u32(i) * 4);
        if (!load_dol_section(cpu, dol, dolSize, "text", i, offset, address, size)) {
            return false;
        }
    }
    for (int i = 0; i < 11; ++i) {
        const u32 offset = read_be32(dol, 0x1C + u32(i) * 4);
        const u32 address = read_be32(dol, 0x64 + u32(i) * 4);
        const u32 size = read_be32(dol, 0xAC + u32(i) * 4);
        if (!load_dol_section(cpu, dol, dolSize, "data", i, offset, address, size)) {
            return false;
        }
    }

    // BSS is left as-is: cpu_init() calloc's cpu->ram, so it already reads
    // as zero, matching what a fresh GameCube boot's BSS clear produces.
    return true;
}

// dr_cpu's CPUState::timebase (extern/dolrecomp/src/cpu/cpu.h) only ever
// changes when guest code explicitly writes it via mtspr (TBL/TBU) -
// nothing advances it on its own, verified reading cpu.c's mftb/mtspr
// handling. Real GameCube hardware's timebase counts continuously at
// OS_TIMER_CLOCK, a real, already-defined constant in this codebase
// (include/dolphin/os.h: OS_BUS_CLOCK/4; extern/aurora/include/dolphin/
// os.h independently confirms OS_BUS_CLOCK as 162,000,000, i.e.
// OS_TIMER_CLOCK = 40,500,000 Hz - not a guess, the decomp's own SDK
// header). Verified hitting this for real: _OSInitAudioSystem spins
// forever calling OSGetTick() in what is clearly a "wait N ticks" delay,
// because with a frozen timebase no amount of elapsed real time ever
// looks like elapsed guest time. Advancing it by real wall-clock elapsed
// time between step_game() calls, at that same rate, is the least
// arbitrary choice: guest ticks pass at the same rate real ticks would.
// ...which this used to do from the host's wall clock, once per frame. That
// is gone: guest time is now derived from consumed guest cycles and synced at
// every MMIO access (sync_guest_timebase, recomp_host.h), because a
// frame-granular clock cannot express the microsecond intervals the SDK's own
// hardware-calibration loops measure - see that header's comment for the
// __AI_SRC_INIT case that proved it. The long-run rate is unchanged: one
// frame's cycle budget below is exactly one frame's worth of timebase ticks
// (kCyclesPerFrame / 12 == 40,500,000 / 60), so OSGetTime() still advances at
// OS_TIMER_CLOCK for a guest that keeps up with 60Hz.

// CPUState::downcount is a cycle budget the *host* owns: generated code only
// ever spends it (`ctx->downcount -= 3;` and friends), and every extracted
// tight loop checks it to decide whether to keep spinning or hand control
// back:
//
//     if (ctx->downcount <= -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET) {
//         ctx->pc = <loop head>; return;   // yield
//     }
//     goto <loop head>;                    // keep going
//
// Nothing here ever refilled it, so it went negative within the first few
// hundred guest cycles of the whole run and stayed there: from then on every
// loop in the game - memset, memcpy, engine loops, wait loops - yielded after
// a single iteration, forever. That is not a hang but a throttle, and a
// brutal one; it is why a plain ~19MB arena memset appeared to freeze the
// boot.
//
// One host time slice = one frame, so refill it with one frame's worth of
// guest CPU cycles. The core clock is OS_BUS_CLOCK * 3 (162MHz bus, 486MHz
// core - the same SDK constant kTimerClockHz above is derived from), and the
// game targets 60Hz.
constexpr u64 kCpuClockHz = 486000000ull;
constexpr s64 kCyclesPerFrame = static_cast<s64>(kCpuClockHz / 60ull);

void refill_slice_budget(CPUState *cpu, s64 cycles)
{
    // Order matters: account for what the slice just ending actually consumed
    // *before* the refill moves downcount, then tell the clock the new value
    // is a refill rather than that many cycles of execution.
    const u64 timebaseBefore = cpu->timebase;
    sync_guest_timebase(cpu);
    // DEC uses the same 40.5MHz units as the timebase (OSAlarm passes its
    // OSTime delta directly to PPCMtdec).  Account before refilling the CPU
    // budget: the elapsed cycles belong to the slice that just finished.
    // A real expiry switches cpu->pc to the guest handler; the next
    // run_blocks call below will execute that handler before normal work.
    interrupt::advance_decrementer(cpu, cpu->timebase - timebaseBefore);
    cpu->downcount = cycles;
    rebase_guest_timebase(cpu);
}

// The SDK's low-memory globals are not part of the DOL: on hardware the
// IPL/apploader writes them before the game's entry point runs, and nothing
// in this port did. A calloc'd guest RAM therefore answered 0 for every one
// of them, and the SDK read those zeros as facts.
//
// That is not a cosmetic gap. OS_BUS_CLOCK is `__OSBusClock` (a variable at
// OS_BASE_CACHED | 0x00F8 - include/dolphin/os.h:66), OS_TIMER_CLOCK is
// OS_BUS_CLOCK / 4, and OSNanosecondsToTicks is
// `((nsec) * (OS_TIMER_CLOCK / 125000)) / 8000`. With the clock at 0, every
// nanosecond-to-tick conversion in the SDK collapses to 0.
//
// Measured consequence: __AI_SRC_INIT (src/dolphin/ai/ai.c) accepts its
// measured interval only if `diff < bound_32KHz - buffer` or
// `bound_32KHz + buffer <= diff < bound_48KHz - buffer`, and with every bound
// converted to 0 both tests are false for any diff whatsoever, so it looped
// forever. The interval itself was already correct - traced at 1271 ticks at
// 32kHz and 843 at 48kHz, exactly the two sample periods - and would have
// passed the first test against a real bound of 1155 ticks.
//
// Only the values this port can state as facts are written here. Anything
// else is left at zero rather than invented.
void install_low_memory_globals(CPUState *cpu)
{
    // include/dolphin/os.h's own constants: the GameCube bus runs at 162MHz
    // and the core at 3x that. Both are fixed by the hardware, not by the
    // disc.
    constexpr u32 kBusClockHz = 162000000u;
    constexpr u32 kCoreClockHz = 486000000u;

    constexpr u32 kOSPhysicalMemSize = 0x80000028u; // __OSPhysicalMemSize
    constexpr u32 kOSSimulatedMemSize = 0x800000F0u; // __OSSimulatedMemSize
    constexpr u32 kOSBusClock = 0x800000F8u;        // __OSBusClock
    constexpr u32 kOSCoreClock = 0x800000FCu;       // __OSCoreClock

    // Whatever this CPUState was actually given, rather than the retail 24MB
    // constant - the two agree today, and if they ever stop agreeing the
    // guest should be told the truth.
    mem_write32(cpu, kOSPhysicalMemSize, cpu->ram_size);
    mem_write32(cpu, kOSSimulatedMemSize, cpu->ram_size);
    mem_write32(cpu, kOSBusClock, kBusClockHz);
    mem_write32(cpu, kOSCoreClock, kCoreClockHz);

    // __OSTVMode (OS_BASE_CACHED | 0x00CC) is deliberately NOT set here. It
    // is equally IPL-provided and equally zero, which reads as VI_NTSC for
    // what is a PAL (GMSP01) disc - a real mismatch, but one that changes the
    // render mode and framebuffer geometry, so it gets its own change and its
    // own measurement rather than riding along with the clocks.
    Log.info("low memory: busClock={} coreClock={} memSize={:#x}", kBusClockHz, kCoreClockHz, cpu->ram_size);
}

// OSBootInfo, and the job this port has been silently skipping: being the
// apploader.
//
// OSBootInfo lives at OS_BASE_CACHED (include/dolphin/os.h:111) and is
// written by the IPL/apploader, not by the DOL. Leaving it zeroed is what
// produced the "bootrom" OSReport this port has printed since the very first
// boot attempt, and that message is not cosmetic - it is DVDInit
// (src/dolphin/dvd/dvd.c:82) branching on `bootInfo->magic`:
//
//     if (magic == 0xE5207C22)       -> booted via JTAG, load the FST itself
//     else if (magic == 0xD15EA5E)   -> booted from bootrom, carry on
//     else { FirstTimeInBootrom = TRUE; OSReport("bootrom\n"); }
//
// That third branch sends the SDK into the full drive bring-up dance -
// inquiry, check ID, spin-up - which this port answers with zeros, so it
// concluded the drive was bad and issued DVDLowStopMotor. Everything
// downstream of that was dead: no FST, so no file ever opened, so every data
// table the game reads was zeros.
//
// So this writes what the apploader would have: the disc ID the SDK compares
// against, the magic that says the disc is already spun up and identified,
// and - the part that actually costs work - the file system table, read off
// the disc and parked in memory with arenaHi lowered to protect it, exactly
// as a real apploader leaves things.
bool install_boot_info(CPUState *cpu)
{
    constexpr u32 kBootInfoBase = 0x80000000u;
    constexpr u32 kBootInfoDiskIdSize = 0x20u;
    constexpr u32 kBootInfoMagicOffset = 0x20u;
    constexpr u32 kBootInfoVersionOffset = 0x24u;
    constexpr u32 kBootInfoConsoleTypeOffset = 0x2Cu;
    constexpr u32 kBootInfoArenaHiOffset = 0x34u;
    constexpr u32 kBootInfoFstLocationOffset = 0x38u;
    constexpr u32 kBootInfoFstMaxLengthOffset = 0x3Cu;

    // "DISEASE" - the value the bootrom leaves behind, per DVDInit's own test.
    constexpr u32 kBootedFromBootrom = 0x0D15EA5Eu;

    // DVDBB2, the disc's own boot block, at the fixed disc offset the SDK's
    // stateCheckID2 reads it from (src/dolphin/dvd/dvd.c: `DVDLowRead(
    // &tmpBuffer, OSRoundUp32B(sizeof(DVDBB2)), 0x420, ...)`). Field offsets
    // from include/dolphin/dvd.h:55.
    constexpr s32 kBootBlockOffset = 0x420;
    constexpr u32 kBootBlockFstPosition = 0x04u;
    constexpr u32 kBootBlockFstLength = 0x08u;
    constexpr u32 kBootBlockFstMaxLength = 0x0Cu;

    const DVDDiskID *diskId = DVDGetCurrentDiskID();
    if (diskId == nullptr) {
        Log.error("boot info: no disc mounted");
        return false;
    }
    const auto *diskIdBytes = reinterpret_cast<const u8 *>(diskId);
    for (u32 i = 0; i < kBootInfoDiskIdSize; ++i) {
        mem_write8(cpu, kBootInfoBase + i, diskIdBytes[i]);
    }

    mem_write32(cpu, kBootInfoBase + kBootInfoMagicOffset, kBootedFromBootrom);
    mem_write32(cpu, kBootInfoBase + kBootInfoVersionOffset, 1);
    // OS_CONSOLE_RETAIL. OSInit overwrites this with OS_CONSOLE_RETAIL1 on its
    // way through anyway; what matters is that the development bit is clear.
    mem_write32(cpu, kBootInfoBase + kBootInfoConsoleTypeOffset, 0);

    // Aurora's DVDReadAbsAsyncPrio wants 32-byte alignment and a 32-byte
    // multiple, the same constraints the real DI DMA has.
    alignas(32) u8 bootBlock[0x20] {};
    DVDCommandBlock block {};
    DVDReadAbsAsyncPrio(&block, bootBlock, sizeof(bootBlock), kBootBlockOffset, nullptr, 2);

    const u32 fstPosition = read_be32(bootBlock, kBootBlockFstPosition);
    const u32 fstLength = read_be32(bootBlock, kBootBlockFstLength);
    const u32 fstMaxLength = read_be32(bootBlock, kBootBlockFstMaxLength);

    if (fstLength == 0 || fstLength > fstMaxLength || fstMaxLength > cpu->ram_size / 4) {
        Log.error("boot info: disc boot block is not usable (fstPosition={:#x} fstLength={:#x} fstMaxLength={:#x})",
            fstPosition, fstLength, fstMaxLength);
        return false;
    }

    // Park the FST at the very top of MEM1 and pull arenaHi down below it,
    // which is what stops the game's own allocator from handing that memory
    // out. OSInit honours BootInfo->arenaHi whenever it is non-null
    // (src/dolphin/os/OS.c:250), so this is the supported way to reserve it
    // rather than a trick.
    const u32 reserved = (fstMaxLength + 31u) & ~31u;
    const u32 fstAddress = (GC_RAM_BASE + cpu->ram_size - reserved) & ~31u;

    std::vector<u8> fst(((fstLength + 31u) & ~31u) + 32u, 0);
    auto *alignedFst = fst.data();
    if (const auto misalignment = reinterpret_cast<uintptr_t>(alignedFst) & 31u; misalignment != 0) {
        alignedFst += 32u - misalignment;
    }
    DVDReadAbsAsyncPrio(&block, alignedFst, static_cast<s32>((fstLength + 31u) & ~31u),
        static_cast<s32>(fstPosition), nullptr, 2);
    for (u32 i = 0; i < fstLength; ++i) {
        mem_write8(cpu, fstAddress + i, alignedFst[i]);
    }

    mem_write32(cpu, kBootInfoBase + kBootInfoFstLocationOffset, fstAddress);
    mem_write32(cpu, kBootInfoBase + kBootInfoFstMaxLengthOffset, fstMaxLength);
    mem_write32(cpu, kBootInfoBase + kBootInfoArenaHiOffset, fstAddress);

    Log.info("boot info: disc {:.4}{:.2} magic=DISEASE, FST {:#x} bytes from disc {:#x} -> {:#010x} (arenaHi)",
        diskId->gameName, diskId->company, fstLength, fstPosition, fstAddress);
    return true;
}

} // namespace

bool boot_game(CPUState *cpu)
{
    DVDDiskID *diskId = DVDGetCurrentDiskID();
    if (diskId == nullptr) {
        Log.error("no disc mounted - open the configured disc image before booting");
        return false;
    }
    if (std::memcmp(diskId->gameName, "GMSP", sizeof(diskId->gameName)) != 0) {
        Log.error("mounted disc's game id doesn't start with GMSP - generated/ was built from a GMSP01 dump, this disc is not it");
        return false;
    }

    s32 dolSize = 0;
    const u8 *dol = DVDGetDOLLocation(&dolSize);
    if (dol == nullptr || dolSize <= 0) {
        Log.error("DVDGetDOLLocation returned no DOL");
        return false;
    }

    if (!cpu_init(cpu)) {
        Log.error("cpu_init failed");
        return false;
    }

    if (!load_dol_into_ram(cpu, dol, u32(dolSize))) {
        cpu_free(cpu);
        return false;
    }

    install_low_memory_globals(cpu);
    if (!install_boot_info(cpu)) {
        cpu_free(cpu);
        return false;
    }

    install_host_calls(cpu);
    install_external_memory(cpu);
    // The Gekko locked cache is plain storage the guest addresses directly, so
    // it is registered as an MMIO range only because it lives outside guest
    // RAM - see kLockedCacheBase.
    g_lockedCache.fill(0);
    g_lcWritesSinceStore.fill(0);
    g_dmaUpper = 0;
    register_mmio_range({
        .base = kLockedCacheBase,
        .end = kLockedCacheBase + kLockedCacheSize,
        .name = "locked cache",
        .read = &locked_cache_read,
        .write = &locked_cache_write,
    });
    interrupt::install();
    interrupt::install_decrementer(cpu);
    cpu->instruction_fallback = &handle_instruction_fallback;
    // PPC_VECTOR_SYSTEM_CALL (sc) is handled directly in step_game(), not
    // through this table - see its own comment for why registering it
    // here like a normal host call doesn't work.

    static const dolphin_sdk::NamedAddress kKnownDolphinSdkCalls[] = {
        { "OSReport", DOLRECOMP_SYMBOL_OSReport },
        // _OSInitAudioSystem was briefly bridged as a full-function skip
        // here, but its poll is reached via an internal goto within the
        // same translated chunk as its caller, not a real dolrecomp_call
        // boundary crossing - a host_call trampoline for its entry address
        // never fires (verified: the hang persisted, identical PC, with
        // this registered). The real fix is the DSPCR self-clearing-bit
        // simulation in include/port/recomp_exi.h's install() instead.
        // DVDInit was tried the same way (IS reached via a real
        // cross-chunk call boundary, unlike those two) and bridging it to
        // Aurora's real no-op DVDInit() DID stop the "bootrom" OSReport
        // loop it was stuck in - but its own caller then called DVDInit
        // again in an immediate, unbroken retry loop instead (same
        // "no-op stub doesn't set a guest-side flag the caller polls for"
        // lesson as EXIInit) - so it's not registered here either.
        // Un-registering it let its own translated body run for real,
        // which is what actually reaches the real EXI channel registers
        // (0xCC0068xx - see include/port/recomp_exi.h's second
        // MmioRangeHandler) that were the real, more specific blocker.
    };
    dolphin_sdk::register_known_dolphin_sdk_calls(kKnownDolphinSdkCalls, std::size(kKnownDolphinSdkCalls));

    // EXI: register-level range (unblocks the confirmed boot hang) plus
    // cheap function-level stubs - see include/port/recomp_exi.h.
    exi::install();
    static const exi::NamedAddress kKnownExiCalls[] = {
        // EXIInit is deliberately not registered here - see
        // src/port/recomp_exi.cpp's register_known_exi_calls for why a
        // no-op stub caused an infinite loop in a real run.
        { "EXIProbe", DOLRECOMP_SYMBOL_EXIProbe },
        { "EXIProbeEx", DOLRECOMP_SYMBOL_EXIProbeEx },
        { "EXIGetState", DOLRECOMP_SYMBOL_EXIGetState },
        { "EXIAttach", DOLRECOMP_SYMBOL_EXIAttach },
    };
    exi::register_known_exi_calls(kKnownExiCalls, std::size(kKnownExiCalls));

    // CARD: bridges to Aurora's real, working implementation - see
    // include/port/recomp_card.h.
    static const card::NamedAddress kKnownCardCalls[] = {
        { "CARDInit", DOLRECOMP_SYMBOL_CARDInit },
        { "CARDMount", DOLRECOMP_SYMBOL_CARDMount },
        { "CARDMountAsync", DOLRECOMP_SYMBOL_CARDMountAsync },
        { "CARDProbeEx", DOLRECOMP_SYMBOL_CARDProbeEx },
        { "CARDCheckExAsync", DOLRECOMP_SYMBOL_CARDCheckExAsync },
        { "CARDCheck", DOLRECOMP_SYMBOL_CARDCheck },
        { "CARDFreeBlocks", DOLRECOMP_SYMBOL_CARDFreeBlocks },
        { "CARDFormat", DOLRECOMP_SYMBOL_CARDFormat },
        { "CARDOpen", DOLRECOMP_SYMBOL_CARDOpen },
        { "CARDClose", DOLRECOMP_SYMBOL_CARDClose },
        { "CARDCreateAsync", DOLRECOMP_SYMBOL_CARDCreateAsync },
        { "CARDCreate", DOLRECOMP_SYMBOL_CARDCreate },
        { "CARDReadAsync", DOLRECOMP_SYMBOL_CARDReadAsync },
        { "CARDRead", DOLRECOMP_SYMBOL_CARDRead },
        { "CARDWriteAsync", DOLRECOMP_SYMBOL_CARDWriteAsync },
        { "CARDWrite", DOLRECOMP_SYMBOL_CARDWrite },
        { "CARDGetStatus", DOLRECOMP_SYMBOL_CARDGetStatus },
        { "CARDSetStatusAsync", DOLRECOMP_SYMBOL_CARDSetStatusAsync },
        { "CARDSetStatus", DOLRECOMP_SYMBOL_CARDSetStatus },
    };
    card::register_known_card_calls(kKnownCardCalls, std::size(kKnownCardCalls));

    // PAD is bridged at the API boundary instead of emulating the SI command
    // DMA below it.  Aurora already owns SDL controller/keyboard input and
    // returns its portable PADStatus; recomp_pad copies its documented
    // 11-byte GameCube prefix back into guest RAM.
    static const pad::NamedAddress kKnownPadCalls[] = {
        { "PADInit", DOLRECOMP_SYMBOL_PADInit },
        { "PADRead", DOLRECOMP_SYMBOL_PADRead },
        { "PADReset", DOLRECOMP_SYMBOL_PADReset },
        { "PADRecalibrate", DOLRECOMP_SYMBOL_PADRecalibrate },
        { "PADControlMotor", DOLRECOMP_SYMBOL_PADControlMotor },
        { "PADSetAnalogMode", DOLRECOMP_SYMBOL_PADSetAnalogMode },
    };
    pad::register_known_pad_calls(kKnownPadCalls, std::size(kKnownPadCalls));

    // GX FIFO: register-level write-gather-pipe forwarding plus the
    // control-plane calls that configure it - see
    // include/port/recomp_gx_fifo.h. GXSetDrawDoneCallback isn't in this
    // dump's symbol table at all - this SDK revision uses GXSetDrawDone
    // instead (bridged below); GXWaitDrawDone has no Aurora
    // implementation and isn't bridged yet.
    gx_fifo::install();
    static const gx_fifo::NamedAddress kKnownGxCalls[] = {
        { "GXInit", DOLRECOMP_SYMBOL_GXInit },
        { "GXSetCPUFifo", DOLRECOMP_SYMBOL_GXSetCPUFifo },
        { "GXSetGPFifo", DOLRECOMP_SYMBOL_GXSetGPFifo },
        { "GXSetDrawDone", DOLRECOMP_SYMBOL_GXSetDrawDone },
        { "GXDrawDone", DOLRECOMP_SYMBOL_GXDrawDone },
        { "GXFlush", DOLRECOMP_SYMBOL_GXFlush },
        { "GXCopyDisp", DOLRECOMP_SYMBOL_GXCopyDisp },
        // Must be bridged, not forwarded: it carries a pointer - see
        // recomp_gx_fifo.cpp's host_call_gx_set_array.
        { "GXSetArray", DOLRECOMP_SYMBOL_GXSetArray },
        // The guest object encodes a GameCube RAM pointer whereas Aurora's
        // texture decoder needs a native pointer. The bridge emits that
        // metadata but deliberately lets the original GX body update gxData.
        { "GXLoadTexObj", DOLRECOMP_SYMBOL_GXLoadTexObj },
        { "GXLoadTexObjPreLoaded", DOLRECOMP_SYMBOL_GXLoadTexObjPreLoaded },
        { "GXInvalidateTexAll", DOLRECOMP_SYMBOL_GXInvalidateTexAll },
    };
    gx_fifo::register_known_gx_calls(kKnownGxCalls, std::size(kKnownGxCalls));

    // MSR[FP] has to be on before the first guest instruction runs. A
    // zero-initialised CPUState leaves it off, and generated code guards
    // every floating-point instruction with ppc_fp_available_inline(), which
    // raises PPC_EXC_FP_UNAVAILABLE and sends pc to the 0x800 vector the
    // moment one is reached - measured: "CPU exception 0x20 ... at
    // srr0=0x80011e94, vector pc=0x00000800", after exactly two step_game()
    // calls, with nothing translated at that physical vector address to
    // recover from it. On hardware this never happens to a game: the
    // apploader runs with the FPU enabled and hands control over with
    // MSR[FP] already set, which is what the SDK's own startup assumes.
    // Setting it here reproduces that entry state rather than emulating an
    // exception handler that the DOL does not contain.
    // MSR[FP] spelled out rather than via PPC_MSR_FP, for the same reason
    // extern/dolrecomp's own cpu.h does: that macro lives in cpu.c, not in
    // any header this can include.
    cpu->msr |= 0x00002000u; // MSR[FP], PPC bit 18

    cpu->pc = DOLRECOMP_ENTRY_POINT;
    Log.info("boot: loaded {} byte DOL, entry={:#010x} msr={:#010x}", dolSize, cpu->pc, cpu->msr);
    return true;
}

bool step_game(CPUState *cpu, unsigned maxBlocks)
{
    // Read-only window into the guest's own OS state - see recomp_probe.h. It
    // throttles and caps itself; calling it unconditionally here keeps the
    // decision about *when* to report in one place.
    probe::report(cpu);

    // One call to step_game is one guest frame, so this is the vertical
    // retrace. It used to be one *host* frame as well, which made the retrace
    // rate the monitor's refresh rate; portmain.cpp now paces these calls from
    // real elapsed time instead, so the guest sees 60 retraces per real second
    // whatever the display does. This only raises the hardware line; when the
    // guest actually takes it is up to MSR[EE] and PI's mask, exactly as on
    // real hardware.
    interrupt::raise_vi_retrace(cpu);

    // The frame's work is run in slices rather than one go, because the host
    // can only deliver an interrupt between slices - so the slice length *is*
    // the interrupt latency, and a whole frame of it would mean no device but
    // VI could ever be serviced in time. Both budgets are divided, since a
    // slice ends on whichever runs out first: the cycle budget (which is what
    // an extracted tight loop yields on) or the block count (which is what
    // bounds straight-line code).
    constexpr unsigned kSlicesPerFrame = 64;
    const unsigned blocksPerSlice = maxBlocks / kSlicesPerFrame > 0 ? maxBlocks / kSlicesPerFrame : 1;

    // How much guest time a frame actually buys, measured rather than assumed.
    //
    // The comment above says a slice ends on whichever budget runs out first.
    // That is not true: dolrecomp_run_blocks (generated.h:621) counts blocks
    // and never looks at downcount, so the block count is the ONLY stop
    // condition and the cycle budget only feeds the clock. A frame should
    // advance the timebase by kCyclesPerFrame/12 = 675,000 ticks; anything
    // less means the guest is being cut off mid-frame and is living in slow
    // motion, which is what the THP decoder running at half its declared
    // 29.97fps looks like from the inside.
    const u64 frameTimebaseStart = cpu->timebase;
    const u64 frameBlocksStart = g_blocksThisFrame;
    unsigned slicesExhausted = 0;
    unsigned slicesIdle = 0;

    for (unsigned slice = 0; slice < kSlicesPerFrame; ++slice) {
        refill_slice_budget(cpu, kCyclesPerFrame / kSlicesPerFrame);
        // Time-driven device work before delivery, so a completion raised here
        // is taken in this same slice rather than waiting for the next one.
        exi::tick(cpu);
        const bool dispatched = deliver_pending_interrupts(cpu);

        // Fast-forward the guest's idle spin.
        //
        // SelectThread parks a guest with nothing runnable in
        //     while (RunQueueBits == 0) ;
        // (src/dolphin/os/OSThread.c), which costs a real GameCube nothing and
        // costs this port full host cycles, because the cycle budget now makes
        // the guest actually sit there rather than being cut off. RunQueueBits
        // is a single guest global (.sbss:0x80405ED8,
        // config/GMSP01/symbols.txt:30995), so recognising the state needs no
        // pc heuristics and no guesswork about which block the spin compiled to.
        //
        // The clock still advances: spending the slice's budget without
        // executing leaves the next refill_slice_budget to account for exactly
        // the same cycles it would have, so guest time passes at the same rate
        // and every time-driven device completion still lands where it would
        // have. What is skipped is only the host work of simulating a spin
        // whose entire purpose is to wait.
        //
        // Never skipped when an interrupt was just delivered: that slice has a
        // handler to run, and it is the handler that makes a thread runnable.
        // RunQueueBits == 0 on its own is NOT idleness, and using it alone
        // deadlocked the boot: before __OSThreadInit runs there is no
        // scheduler and no runnable thread to count, so the condition held
        // from the first frame and every slice of every frame was skipped -
        // measured as "64 slices idle-skipped" with the game frozen at
        // appState=0 (WAIT).
        //
        // The exact signal is the context. SelectThread does
        // OSSetCurrentContext(&IdleContext) immediately before the spin and
        // OSClearContext(&IdleContext) immediately after, so
        // __OSCurrentContext pointing at IdleContext means the scheduler is up
        // AND has nothing to run - which cannot be true before it exists.
        // RunQueueBits is kept as the second half because a non-zero value
        // means the spin is about to exit anyway.
        const bool guestIsIdle = mem_read32(cpu, kOsCurrentContextAddress) == kIdleContextAddress
            && mem_read32(cpu, kRunQueueBitsAddress) == 0;
        if (!dispatched && guestIsIdle) {
            cpu->downcount = 0;
            ++slicesIdle;
            continue;
        }

        switch (run_blocks(cpu, blocksPerSlice)) {
            case RunStop::BudgetExhausted:
                if (g_lastSliceHitBlockCap) {
                    ++slicesExhausted;
                }
                continue;
            case RunStop::Exception:
                Log.warn("step_game: CPU exception {:#x} (program_exception cause={:#x}) at srr0={:#010x}, vector pc={:#010x}",
                    cpu->exception, cpu->program_exception, cpu->srr0, cpu->pc);
                return false;
            case RunStop::Halted:
                Log.warn("step_game: halted at pc={:#010x} (unresolved call)", cpu->pc);
                return false;
            case RunStop::RepeatedSystemCalls:
                Log.warn("step_game: gave up after repeated system-call traps near pc={:#010x}", cpu->pc);
                return false;
        }
    }

    {
        // One line per second, like every other probe here.
        static u64 lastReport = 0;
        constexpr u64 kTicksPerSecond = 40500000ull;
        constexpr u64 kExpectedTicksPerFrame = kCyclesPerFrame / 12; // 12 core cycles per timebase tick (recomp_host.cpp:81)
        if (cpu->timebase - lastReport >= kTicksPerSecond) {
            lastReport = cpu->timebase;
            const u64 advanced = cpu->timebase - frameTimebaseStart;
            const u64 blocks = g_blocksThisFrame - frameBlocksStart;
            Log.info("frame budget: guest time advanced {} of {} ticks ({:.0f}%) | {} of {} slices stopped on the "
                     "block cap rather than on cycles | {} slices idle-skipped | blocksPerSlice={} | {} blocks, "
                     "{:.1f} guest cycles per block",
                advanced, kExpectedTicksPerFrame, 100.0 * double(advanced) / double(kExpectedTicksPerFrame),
                slicesExhausted, kSlicesPerFrame, slicesIdle, blocksPerSlice, blocks,
                blocks == 0 ? 0.0 : double(advanced * 12u) / double(blocks));
        }
    }
    return true;
}

bool locked_cache_store_covered(u32 guestAddr, u32 bytes, bool *recentEnough)
{
    if (recentEnough != nullptr) {
        *recentEnough = g_lcStoreTotal <= kLockedCacheStoreRing;
    }
    if (bytes == 0) {
        return true;
    }

    // Every locked-cache transfer is a whole number of 32-byte blocks at a
    // 32-byte-aligned address, so tracking coverage at block granularity is
    // exact rather than approximate.
    const u32 firstBlock = guestAddr / 32u;
    const u32 lastBlock = (guestAddr + bytes + 31u) / 32u;
    std::vector<bool> covered(lastBlock - firstBlock, false);

    for (const auto &store : g_lcStores) {
        if (store.bytes == 0) {
            continue;
        }
        const u32 storeFirst = store.guestBase / 32u;
        const u32 storeLast = (store.guestBase + store.bytes + 31u) / 32u;
        const u32 from = storeFirst > firstBlock ? storeFirst : firstBlock;
        const u32 to = storeLast < lastBlock ? storeLast : lastBlock;
        for (u32 block = from; block < to; ++block) {
            covered[block - firstBlock] = true;
        }
    }

    for (const bool bit : covered) {
        if (!bit) {
            return false;
        }
    }
    return true;
}

bool run_locked_cache_self_test()
{
    CPUState cpu {};
    if (!cpu_init(&cpu)) {
        Log.error("locked-cache self-test: cpu_init failed");
        return false;
    }

    g_lockedCache.fill(0);
    g_lcWritesSinceStore.fill(0);
    g_lcStores.fill({});
    g_lcStoreNext = 0;
    g_lcStoreTotal = 0;

    // The cache is guest-visible big-endian storage.  Exercise an unaligned
    // 32-bit write as well as byte reads before involving DMA, so a byte-order
    // regression cannot hide behind a symmetric copy.
    locked_cache_write(&cpu, kLockedCacheBase + 3u, 0x11223344u, 4);
    if (locked_cache_read(&cpu, kLockedCacheBase + 3u, 4) != 0x11223344u
        || locked_cache_read(&cpu, kLockedCacheBase + 4u, 1) != 0x22u) {
        Log.error("locked-cache self-test: guest byte order mismatch");
        cpu_free(&cpu);
        return false;
    }

    // THP's IDCT does not write its pixels one byte at a time.  It uses
    // psq_st with GQR6 (unsigned-byte quantisation, scale -3) directly into
    // the locked-cache address range.  Exercise that exact route through
    // dr_cpu's external-memory callbacks: this catches a regression where
    // normal cache reads/writes work but paired-single output is silently
    // routed to unmapped memory or byte-swapped.
    cpu.external_read = [](CPUState *state, u32 address, u8 size) -> u64 {
        return locked_cache_read(state, address, size);
    };
    cpu.external_write = [](CPUState *state, u32 address, u64 value, u8 size) {
        locked_cache_write(state, address, value, size);
    };
    cpu.hid2 = PPC_HID2_PSE | PPC_HID2_LSQE;
    cpu.fpr[0] = 12.0;
    cpu.ps1[0] = 200.0;
    // GQR6's load/store fields are both type U8 with signed scale -3, the
    // value installed by __THPGQRSetup in the real decoder.
    cpu.gqr[6] = 0x3D043D04u;
    if (!ppc_psq_store(&cpu, 0, kLockedCacheBase + 0x80u, false, 6, false, 0)
        || locked_cache_read(&cpu, kLockedCacheBase + 0x80u, 1) != 1u
        || locked_cache_read(&cpu, kLockedCacheBase + 0x81u, 1) != 25u) {
        Log.error("locked-cache self-test: GQR6 paired-single store mismatch");
        cpu_free(&cpu);
        return false;
    }
    if (!ppc_psq_load(&cpu, 1, kLockedCacheBase + 0x80u, false, 6, false, 0)
        || cpu.fpr[1] != 8.0 || cpu.ps1[1] != 200.0) {
        Log.error("locked-cache self-test: GQR6 paired-single load mismatch");
        cpu_free(&cpu);
        return false;
    }

    constexpr u32 kStoreMainOffset = 0x1000u;
    constexpr u32 kStoreLcOffset = 0x200u;
    constexpr u32 kBlocks = 2u;
    constexpr u32 kBytes = kBlocks * 32u;
    for (u32 i = 0; i < kBytes; ++i) {
        locked_cache_write(&cpu, kLockedCacheBase + kStoreLcOffset + i, static_cast<u8>(i ^ 0xA5u), 1);
    }
    g_dmaUpper = kStoreMainOffset | (kBlocks >> 2);
    perform_locked_cache_dma(&cpu, kLockedCacheBase + kStoreLcOffset | ((kBlocks & 3u) << 2));
    for (u32 i = 0; i < kBytes; ++i) {
        if (mem_read8(&cpu, GC_RAM_BASE + kStoreMainOffset + i) != static_cast<u8>(i ^ 0xA5u)) {
            Log.error("locked-cache self-test: store DMA mismatch at byte {}", i);
            cpu_free(&cpu);
            return false;
        }
    }
    bool recentEnough = false;
    if (!locked_cache_store_covered(GC_RAM_BASE + kStoreMainOffset, kBytes, &recentEnough) || !recentEnough) {
        Log.error("locked-cache self-test: store coverage was not recorded");
        cpu_free(&cpu);
        return false;
    }

    constexpr u32 kLoadMainOffset = 0x1800u;
    constexpr u32 kLoadLcOffset = 0x400u;
    for (u32 i = 0; i < kBytes; ++i) {
        mem_write8(&cpu, GC_RAM_BASE + kLoadMainOffset + i, static_cast<u8>(0x5Au - i));
    }
    g_dmaUpper = kLoadMainOffset | (kBlocks >> 2);
    perform_locked_cache_dma(&cpu, kLockedCacheBase + kLoadLcOffset | ((kBlocks & 3u) << 2) | kDmaLoadBit);
    for (u32 i = 0; i < kBytes; ++i) {
        if (locked_cache_read(&cpu, kLockedCacheBase + kLoadLcOffset + i, 1) != static_cast<u8>(0x5Au - i)) {
            Log.error("locked-cache self-test: load DMA mismatch at byte {}", i);
            cpu_free(&cpu);
            return false;
        }
    }

    cpu_free(&cpu);
    Log.info("locked-cache self-test: guest storage plus load/store DMA paths passed");
    return true;
}

bool run_fpu_context_self_test()
{
    CPUState cpu {};
    if (!cpu_init(&cpu)) {
        Log.error("FPU context self-test: cpu_init failed");
        return false;
    }

    constexpr u32 kContextA = GC_RAM_BASE + 0x1000u;
    constexpr u32 kContextB = GC_RAM_BASE + 0x1400u;
    mem_write32(&cpu, kOsCurrentContextAddress, kContextB);
    mem_write32(&cpu, kOsFpuContextAddress, kContextA);
    cpu.fpr[3] = 12.5;
    cpu.ps1[3] = -7.25;
    cpu.fpscr = 0x12345678u;

    // B has not used FP before. Moving ownership saves A but intentionally
    // leaves the architectural FP register file alone, matching
    // __OSLoadFPUContext's no-op for a context without FPSAVED.
    if (!handle_fp_unavailable(&cpu)
        || (mem_read16(&cpu, kContextA + kOsContextStateOffset) & kOsContextStateFpSaved) == 0
        || mem_read64(&cpu, kContextA + kOsContextFprOffset + 3u * 8u)
            != dolrecomp_f64_to_bits(12.5)
        || mem_read64(&cpu, kContextA + kOsContextPsfOffset + 3u * 8u)
            != dolrecomp_f64_to_bits(-7.25)
        || static_cast<u32>(mem_read64(&cpu, kContextA + kOsContextFpscrOffset)) != 0x12345678u) {
        Log.error("FPU context self-test: saving the prior owner failed");
        cpu_free(&cpu);
        return false;
    }

    cpu.fpr[3] = -3.0;
    cpu.ps1[3] = 42.0;
    cpu.fpscr = 0x89ABCDEFu;
    mem_write32(&cpu, kOsCurrentContextAddress, kContextA);
    if (!handle_fp_unavailable(&cpu)
        || cpu.fpr[3] != 12.5 || cpu.ps1[3] != -7.25 || cpu.fpscr != 0x12345678u
        || mem_read32(&cpu, kOsFpuContextAddress) != kContextA
        || mem_read64(&cpu, kContextB + kOsContextFprOffset + 3u * 8u)
            != dolrecomp_f64_to_bits(-3.0)
        || mem_read64(&cpu, kContextB + kOsContextPsfOffset + 3u * 8u)
            != dolrecomp_f64_to_bits(42.0)) {
        Log.error("FPU context self-test: restoring the next owner failed");
        cpu_free(&cpu);
        return false;
    }

    cpu_free(&cpu);
    Log.info("FPU context self-test: lazy per-thread FPR/PS/FPSCR handoff passed");
    return true;
}

} // namespace sms::recomp

#else // !DOLPHINJET_HAVE_RECOMPILED_GAME

namespace sms::recomp {

bool boot_game(CPUState *)
{
    return false;
}

bool step_game(CPUState *, unsigned)
{
    return false;
}

bool locked_cache_store_covered(u32, u32, bool *recentEnough)
{
    if (recentEnough != nullptr) {
        *recentEnough = false;
    }
    return false;
}

bool run_locked_cache_self_test()
{
    return false;
}

bool run_fpu_context_self_test()
{
    return false;
}

} // namespace sms::recomp

#endif
