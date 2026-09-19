#include "port/recomp_exi.h"
#include "port/recomp_host.h"
#include "port/recomp_interrupt.h"

#include "aurora/lib/logging.hpp"

#include <dolphin/dvd.h>

#include <array>
#include <cstring>
#include <deque>
#include <functional>
#include <unordered_set>
#include <vector>

namespace sms::recomp::exi {
namespace {

aurora::Module Log("sms::recomp::exi");

// Starting range: the confirmed hang (docs/port_bootstrap.md) is reads
// around 0xCC00500A. 0x40 bytes gives headroom for a handful of
// neighboring 32-bit registers either side without guessing an exact
// per-channel stride - narrow/widen once a real run's log (below) shows
// exactly which offsets are actually touched.
//
// Correction from a real run, superseding this file's name: this address
// range is not actually EXI. The PC logged alongside every access
// (verified via cpu->pc, see read()/write() below) lands inside
// _OSInitAudioSystem's own symbol range (generated/generated_symbols.h:
// DOLRECOMP_SYMBOL__OSInitAudioSystem 0x8033B534, size 0x1BC) - this is
// the GameCube DSP interface block (DSPCR and neighbors), polled during
// audio bring-up, not memory-card/EXI probing. Kept in this file/module
// for now rather than renamed mid-session, since the mechanism (a shadow
// register file over an unmodeled low-CC0 hardware range) is identical
// either way - only the specific address and bit semantics differ.
constexpr u32 kRangeBase = 0xCC005000u;
constexpr u32 kRangeEnd = 0xCC005040u;
constexpr size_t kWordCount = (kRangeEnd - kRangeBase) / 4;

// Plain read-what-was-written shadow state, one 32-bit word per register
// offset in the range - not a guess at hardware semantics by itself, just
// "memory instead of always-zero". Every access is logged (see read/write
// below) so a real run's actual address/value pattern is directly visible,
// rather than silently trusting the heuristic below.
std::array<u32, kWordCount> g_shadow {};

// ---------------------------------------------------------------------------
// ARAM (the GameCube's 16MB auxiliary RAM), reached through this same DSP
// register block. Boot stops dead without it: ARInit() -> __ARChecksize()
// (src/dolphin/ar/ar.c, real decompiled SDK source, not inferred) opens with
//
//     do { } while (!(__DSPRegs[DSP_ARAM_MODE] & 1));
//
// and then sizes ARAM the way hardware requires - DMA a known pattern out to
// several addresses at and beyond 16MB, read it back, and infer the size from
// which reads alias onto which writes. A shadow register file answers 0 to
// all of it, so the mode poll never exits (measured: pc parked at 0x8034B2C8,
// 0x18 into __ARChecksize, burning the whole cycle budget every frame).
//
// So this models ARAM as what it is: a flat 16MB buffer plus the DMA engine
// that moves bytes between it and main memory. The wrap-around below is the
// load-bearing part - real ARAM aliases addresses past its end back to the
// start, and that aliasing is precisely the signal __ARChecksize reads the
// size from. Emulating it faithfully lets the SDK's own probe reach its own
// conclusion, instead of this code trying to hand it an answer.
//
// __DSPRegs is u16[], so register index i sits at byte offset 2i: even i in
// the high half of a shadow word, odd i in the low half. Indices from
// include/dolphin/hw_regs.h.
constexpr u32 kAramSize = 0x1000000u; // 16MB, retail GameCube
std::vector<u8> g_aram;

constexpr size_t kDspWordControlStatus = 2; // reg 5 (DSPCR), low half at 0x0A
constexpr size_t kDspWordAramMode = 5;    // reg 11 (DSP_ARAM_MODE), low half
constexpr u32 kAramModeReadyBit = 1u << 0;
constexpr size_t kDspWordDmaMainMem = 8;  // regs 16/17, main-memory address
constexpr size_t kDspWordDmaAram = 9;     // regs 18/19, ARAM address
constexpr size_t kDspWordDmaSize = 10;    // regs 20/21, length + direction
constexpr u32 kDspDmaSizeLowOffset = 0x2Au; // reg 21, the write that starts it
// SIZE_HI bit 15 - clear means main memory -> ARAM, set means ARAM -> main
// memory (__ARWriteDMA clears it, __ARReadDMA sets it, ar.c).
constexpr u32 kDspDmaToMainMemBit = 0x80000000u;
// Only the low 10 bits of each *_HI register are address/length bits, which
// is also what strips the 0x8 nibble off a cached pointer and leaves the
// physical address hardware DMA actually uses.
constexpr u32 kDspDmaHighMask = 0x03FFu;

// DSPCR's three interrupt status bits, decoded by this chain in
// __OSDispatchInterrupt (src/dolphin/os/OSInterrupt.c):
//
//     reg = __DSPRegs[5];
//     if (reg & 0x8)  cause |= OS_INTERRUPTMASK_DSP_AI;
//     if (reg & 0x20) cause |= OS_INTERRUPTMASK_DSP_ARAM;
//     if (reg & 0x80) cause |= OS_INTERRUPTMASK_DSP_DSP;
//
// __DSPRegs is a u16* so index 5 is the halfword at 0xCC00500A, the low half
// of the word this file calls kDspWordControlStatus. All three are
// write-one-to-clear and all three share PI's single DSP cause bit.
constexpr u32 kDspInterruptStatusBit = 0x0080u;
constexpr u32 kDspAramInterruptStatusBit = 0x0020u;
constexpr u32 kDspAiInterruptStatusBit = 0x0008u;
constexpr u32 kDspAllInterruptStatusBits
    = kDspInterruptStatusBit | kDspAramInterruptStatusBit | kDspAiInterruptStatusBit;

// Every handler in this file logs each register access, which is exactly the
// right amount of detail the first time a given access happens and a disaster
// the millionth: guest code polls these registers in tight loops (ARAM mode,
// DSP mailbox, EXI/DI completion), so an unthrottled INFO line per access
// filled a 55GB log file and ran the disk out of space mid-session. Keyed on
// address+value, so a poll that keeps reading the same thing is logged once
// while a value that actually changes still shows up.
bool first_time_seeing(const char *kind, u32 addr, u64 value)
{
    struct Seen {
        const char *kind;
        u32 addr;
        u64 value;
        bool operator==(const Seen &) const = default;
    };
    struct Hash {
        size_t operator()(const Seen &s) const noexcept
        {
            return std::hash<const void *> {}(static_cast<const void *>(s.kind))
                ^ (std::hash<u32> {}(s.addr) << 1) ^ (std::hash<u64> {}(s.value) << 2);
        }
    };
    static std::unordered_set<Seen, Hash> seen;
    return seen.insert(Seen { kind, addr, value }).second;
}

u32 dsp_dma_field(size_t wordIndex)
{
    const u32 word = g_shadow[wordIndex];
    return (((word >> 16) & kDspDmaHighMask) << 16) | (word & 0xFFFFu);
}

void perform_aram_dma(CPUState *cpu)
{
    const u32 mainMem = dsp_dma_field(kDspWordDmaMainMem);
    const u32 aramAddr = dsp_dma_field(kDspWordDmaAram);
    const u32 length = dsp_dma_field(kDspWordDmaSize);
    const bool toMainMem = (g_shadow[kDspWordDmaSize] & kDspDmaToMainMemBit) != 0;

    Log.info("ARAM DMA {} mram={:#010x} aram={:#010x} len={:#x} pc={:#010x}",
        toMainMem ? "ARAM->MRAM" : "MRAM->ARAM", mainMem, aramAddr, length, cpu->pc);

    for (u32 i = 0; i < length; ++i) {
        const u32 aramOffset = (aramAddr + i) & (kAramSize - 1u);
        const u32 guestAddr = ((mainMem + i) & 0x03FFFFFFu) | GC_RAM_BASE;
        if (toMainMem) {
            mem_write8(cpu, guestAddr, g_aram[aramOffset]);
        } else {
            g_aram[aramOffset] = mem_read8(cpu, guestAddr);
        }
    }

    // The transfer is synchronous here, but "finished" still has to be
    // announced the way hardware announces it. ARStartDMA's caller gets its
    // completion through the callback ARRegisterDMACallback installed, and
    // that callback only ever runs from the ARAM DMA interrupt - so a DMA that
    // moved every byte correctly and raised nothing leaves the caller waiting
    // forever. Measured: JASystem::Dvd::loadToAramDvdTMain streamed 30 reads
    // off the disc, performed 12 ARAM DMAs, and then sat in the same place for
    // thousands of frames.
    g_shadow[kDspWordControlStatus] |= kDspAramInterruptStatusBit;
    interrupt::raise(interrupt::kCauseDsp);
}

// True when a write of `size` bytes at `addr` covers the DSP_ARAM_DMA_SIZE_LO
// halfword, which is the last register ar.c writes and therefore what starts
// the transfer.
bool write_starts_aram_dma(u32 addr, u8 size)
{
    const u32 start = addr - kRangeBase;
    return start <= kDspDmaSizeLowOffset && kDspDmaSizeLowOffset < start + size;
}

// Second correction from the same real run: the first hypothesis here
// (bit 0 = start, bit 2 = completion) was wrong - the game kept spinning
// even with bit 2 already set. The actual observed sequence at 0xCC00500A
// was write 0x8ac, read back 0x8ac, write 0x8ad (only bit 0 changed, 0->1),
// then an unbroken read-only spin loop on the same address forever - the
// classic "write a RES/reset-trigger bit, then poll until hardware
// self-clears it once the reset completes" pattern (GameCube's
// documented DSP Control Register, DSPCR, has exactly such a
// self-clearing bit 0). A write-only shadow that never clears bit 0 on
// its own can never satisfy that poll. Corrected hypothesis: reading this
// range always reports bit 0 as already clear, simulating an
// instantaneous reset - the shadow still stores whatever was written
// (including bit 0) so other bits round-trip normally, only bit 0 is
// forced clear on read.
constexpr u32 kSelfClearingResetBit = 1u << 0;

// Third correction, this time from reading the actual generated C code
// (generated/chunks/chunk_0206_text1_80339600.c) around both hang points
// directly, rather than inferring from log patterns alone - the previous
// "always report bit 15 set" fix was only half right and created a new
// hang of its own once the game read the SAME bit through a second loop.
//
// Per the publicly documented GameCube DSP interface register layout,
// 0xCC005000/5002 is the CPU->DSP mailbox (high/low halves) and
// 0xCC005004/5006 is the DSP->CPU mailbox (high/low), bit 15 of each
// high half being the documented "mailbox valid" flag. Two distinct
// loops poll it with *opposite* polarity - real evidence, not inferred:
//   - loop_8033B688 (label_8033B688): reads only 0xCC005004, masks to
//     bit 15, and loops *while that bit is 0* (`bc 12,2,...` = beq-style
//     branch-if-CR0.EQ against a zero-masked result) - i.e. it waits for
//     the bit to become SET.
//   - loop_8033B5B8 (label_8033B5B8): reads BOTH 0xCC005004 and
//     0xCC005006, combines them into one 32-bit value, masks to the same
//     bit (now at position 31), and loops *while that bit is 1*
//     (`bc 4,2,...` = bne-style branch) - i.e. it waits for the bit to
//     become CLEAR again.
// This looks like the standard hardware "read-clears" mailbox
// convention at first - and a read-clear-on-low-half simulation of
// exactly that was tried - but a real run showed the two loops don't run
// in the order the static code's layout suggests: loop_8033B5B8 runs
// (and, under read-clear, consumes the response) *before*
// loop_8033B688's own later check ever sees it, leaving loop_8033B688
// stuck waiting for a *second* response - almost certainly a real,
// separate DSP-initiated message this CPU-side code alone doesn't
// explain. Rather than guess at that second message's trigger, this
// stays intentionally simple: the valid bit reads as permanently set
// (see install() below) once installed - satisfies loop_8033B688
// immediately and lets loop_8033B5B8 through as many times as it's
// reached, at the cost of not modeling the real handshake precisely.
constexpr u32 kDspToCpuMailboxHigh = 0xCC005004u;
constexpr u32 kDspToCpuMailboxLow = 0xCC005006u;
bool g_mailboxResponsePending = false;

// The other direction: 0xCC005000/5002 is the CPU->DSP mailbox, and bit 15 of
// its high half is the "mail waiting to be collected" flag. DSPSendMailToDSP
// writes the high half first, so that bit arrives set by construction (every
// mail __DSP_boot_task sends has its top bit set - 0x80F3A001, 0x80F3C002 and
// so on), and on hardware the DSP clears it when it reads the mail.
//
// There is no DSP here, so a plain shadow left it set forever and every
// `while (DSPCheckMailToDSP() != 0) ;` in src/dolphin/dsp/dsp_task.c spun
// forever - measured as pc parked at 0x8034C2D0, inside __DSP_boot_task
// (0x8034C28C), for every frame of a 570-frame run.
//
// Reporting it clear on read is the same "absent hardware completes
// instantly" simulation this file already applies to DSPCR's self-clearing
// reset bit. It is explicitly NOT a DSP: the mail is accepted and dropped, so
// no microcode ever runs and nothing will come back from one. Real audio has
// to come from bridging the SDK's audio path to the host, not from this.
constexpr size_t kDspWordMailboxToCpuSide = 0; // (0xCC005000 - kRangeBase) / 4
constexpr u32 kDspMailToDspPendingBit = 0x80000000u; // bit 15 of the high half

// The low half of the CPU->DSP mailbox, written second by DSPSendMailToDSP
// (`__DSPRegs[0] = mail >> 16; __DSPRegs[1] = mail & 0xFFFF;`), so a write
// there is the moment a complete mail has been sent.
constexpr u32 kCpuToDspMailboxLow = 0xCC005002u;

// DSPCR's interrupt status bit. __OSDispatchInterrupt turns it into
// OS_INTERRUPTMASK_DSP_DSP (`reg = __DSPRegs[5]; if (reg & 0x80) ...`), and
// __DSPHandler acknowledges by writing it back set - write-one-to-clear, which
// is why it needs handling of its own rather than the plain shadow write.
// __DSPRegs is a u16* so index 5 is the halfword at 0xCC00500A, the low half
// of the word this file already calls kDspWordControlStatus.

// Mail the simulated DSP has posted but the CPU has not read yet. A queue
// rather than a flag because the boot handshake posts two in a row and the
// second is only read from inside the first one's handler.
std::deque<u32> g_dspMailToCpu;

// The last mail __DSP_boot_task sends before the DSP would start running is
// the task's init vector, and the one immediately before that is the fixed
// marker 0x80F3D001 (src/dolphin/dsp/dsp_task.c). Watching for that pair is
// how the end of the upload is recognised without counting mails, which would
// break the moment the sequence changed.
bool g_dspBootAwaitingInitVector = false;

// DSPAssertInt (src/dolphin/dsp/dsp.c) pokes DSPCR bit 1 to tell the DSP a
// command is coming. DSPSendCommands2 sends the command count *before* that
// poke and the command mails after it, so the poke is the boundary between the
// two.
constexpr u32 kDspAssertIntBit = 0x0002u;

// src/JSystem/JAudio/JASystem/JASDSPInterface.cpp: `u16 JAS_DSP_PREFIX =
// 0xF355;`. syncDSP rejects any mail whose high half is not this.
constexpr u32 kJasDspPrefix = 0xF355u;

// The mail the microcode sends when it wants the task's req_cb run
// (src/JSystem/osdsp_task.c's __DSPHandler, case 0xDCD10004).
constexpr u32 kDspRequestMail = 0xDCD10004u;

// Where the command stream is up to. Idle until DSPAssertInt, then counting
// down the mails the guest announced.
enum class DspCommandPhase { Idle, AwaitingCommands };
DspCommandPhase g_dspCommandPhase = DspCommandPhase::Idle;
u32 g_dspCommandsRemaining = 0;
u32 g_dspCommandWorkId = 0;
bool g_dspCommandFirstMail = false;
u32 g_dspLastMailToDsp = 0;

// DSPSendCommands2 announces `param_2` and then sends `param_2 == 0 ? 2 :
// param_2` mails. Clamped because the count comes from guest memory and a
// wild value must not put this into a state it never leaves.
constexpr u32 kDspMaxCommandMails = 16;

size_t word_index(u32 addr)
{
    return (addr - kRangeBase) / 4;
}

u64 read(CPUState *cpu, u32 addr, u8 size)
{
    const size_t index = word_index(addr & ~3u);
    u32 word = index < g_shadow.size() ? g_shadow[index] : 0;
    // Bit 0 always reads clear - see kSelfClearingResetBit's comment - the
    // underlying shadow word still keeps it set (write() doesn't touch
    // it), only the read side simulates the instantaneous self-clear.
    //
    // Scoped to DSPCR's own word. This used to apply to every word in the
    // range, which is wrong twice over: DSPCR is the register documented to
    // self-clear, and bit 0 means something entirely different elsewhere in
    // the block. It silently swallowed ARAM's ready bit (DSP_ARAM_MODE, in
    // word 5), so __ARChecksize's opening poll could never exit no matter
    // what install() seeded - measured as pc parked at 0x8034B2C8 burning a
    // full cycle budget per frame.
    if (index == kDspWordControlStatus) {
        word &= ~kSelfClearingResetBit;
    }
    if (index == kDspWordMailboxToCpuSide) {
        // Mail sent to the DSP is always already collected - see
        // kDspMailToDspPendingBit. The shadow keeps what was written so the
        // value itself still round-trips; only the pending flag is forced
        // down on the read side.
        word &= ~kDspMailToDspPendingBit;
    }

    const bool touchesMailboxWord = (addr & ~3u) == kDspToCpuMailboxHigh;
    if (touchesMailboxWord && !g_dspMailToCpu.empty()) {
        // Real mail, posted by the simulated microcode handshake in write()
        // below. The valid bit is forced on rather than assumed: every mail
        // value the protocol actually uses happens to have its top bit set,
        // but the queue must not depend on that.
        word = g_dspMailToCpu.front() | 0x80000000u;
    } else if (touchesMailboxWord && g_mailboxResponsePending) {
        // 0x80544348: the real GameCube DSP audio-init UCode's boot
        // acknowledgment value - not guessed, this is Dolphin emulator's
        // own DSP HLE INIT UCode (Source/Core/Core/HW/DSPHLE/UCodes/
        // INIT.cpp, INITUCode::Initialize(): m_mail_handler.PushMail(
        // 0x80544348)), which real games' _OSInitAudioSystem-equivalent
        // code has to accept correctly since Dolphin's HLE is verified
        // against real hardware behavior across the whole game library.
        // Already includes bit 15 of the high half (0x8054 & 0x8000 != 0)
        // as the "mailbox valid" bit - no separate OR needed.
        word = 0x80544348u;
    }

    // Sub-word reads pull the requested bytes out of the containing
    // big-endian 32-bit word, matching how a real register would be
    // byte/halfword-addressable within its word.
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u64 value = (u64(word) >> shift) & ((size == 4) ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1));
    if (first_time_seeing("dsp-read", addr, value)) {
        Log.info("read  {:#010x} (size={}) -> {:#x} [word {:#010x}] pc={:#010x}", addr, size, value, word, cpu->pc);
    }

    // The low half is the "consuming" read - loop_8033B5B8 reads both
    // halves together and waits for exactly this to bring the valid bit
    // back down (real read-clears mailbox hardware behavior) before it
    // proceeds - see the comment above g_mailboxResponsePending for why
    // this alone isn't sufficient and the DSPCR trigger in write() below
    // is also needed.
    if (touchesMailboxWord && (addr == kDspToCpuMailboxLow || (addr == kDspToCpuMailboxHigh && size == 4))) {
        if (!g_dspMailToCpu.empty()) {
            g_dspMailToCpu.pop_front();
        } else {
            g_mailboxResponsePending = false;
        }
    }
    return value;
}

void write(CPUState *cpu, u32 addr, u64 value, u8 size)
{
    const size_t index = word_index(addr & ~3u);
    if (index >= g_shadow.size()) {
        return;
    }
    u32 &word = g_shadow[index];
    const u32 oldWord = word;
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u32 mask = (size == 4 ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1)) << shift;
    word = (word & ~mask) | ((u32(value) << shift) & mask);
    if (first_time_seeing("dsp-write", addr, value)) {
        Log.info("write {:#010x} (size={}) <- {:#x} [word now {:#010x}] pc={:#010x}", addr, size, value, word, cpu->pc);
    }

    // The DSPHalt bit (bit 2 of DSPCR, per Dolphin emulator's own
    // UDSPControl struct - Source/Core/Core/HW/DSP.h) transitioning from
    // set to clear is the CPU starting the DSP running after upload -
    // verified from the real generated code (chunk_0206_text1_80339600.c,
    // labels 0x8033B674-0x8033B680): it read-modify-writes DSPCR clearing
    // exactly this bit right between loop_8033B5B8 (consumes the first
    // mailbox response) and loop_8033B688 (waits for a *second* one),
    // with no fresh CPU->DSP mailbox write of its own in between. Real
    // hardware's DSP would, once started, asynchronously send its own
    // second "I'm up" message on its own - simulated here as: clearing
    // DSPHalt re-arms the mailbox valid flag, standing in for that
    // self-initiated message since nothing here has a real DSP core to
    // send one for real.
    constexpr u32 kDspHaltBit = 1u << 2;
    if ((addr & ~3u) == kRangeBase + 0x08u && (oldWord & kDspHaltBit) && !(word & kDspHaltBit)) {
        g_mailboxResponsePending = true;
        Log.info("  (DSPHalt cleared -> re-arming mailbox valid, simulating the DSP's own started-running message)");
    }

    // Writing the CPU->DSP mailbox's high half is the "send a command"
    // step - simulate the DSP responding instantly (nothing here has a
    // real DSP core to respond for real, or to know what it should say -
    // see the comment above g_mailboxResponsePending).
    if ((addr & ~3u) == kRangeBase) {
        g_mailboxResponsePending = true;
    }

    // DSPCR's interrupt status bit is write-one-to-clear, so it cannot go
    // through the plain shadow write above: writing a 0 there must leave it
    // alone, and writing a 1 must drop it - and with it PI's DSP cause, or
    // __OSDispatchInterrupt would keep re-entering the same interrupt.
    // __DSPHandler acknowledges exactly this way (`temp |= 0x80;
    // __DSPRegs[5] = temp;`, src/JSystem/osdsp_task.c).
    if (index == kDspWordControlStatus && (mask & kDspAllInterruptStatusBits) != 0) {
        const u32 written = (u32(value) << shift) & mask;
        const u32 covered = mask & kDspAllInterruptStatusBits;
        // Write-one-to-clear: bits written as 1 drop, bits written as 0 keep
        // whatever they had.
        word &= ~(written & covered);
        word |= (oldWord & covered & ~written);
        if ((word & kDspAllInterruptStatusBits) == 0) {
            // Nothing outstanding from this block any more, so PI must stop
            // reporting it - all three share one cause bit.
            interrupt::clear(interrupt::kCauseDsp);
        }
    }

    // DSPAssertInt: the command count has just been sent, the command mails
    // are about to be. See kDspAssertIntBit.
    if (index == kDspWordControlStatus && (mask & kDspAssertIntBit) != 0
        && (((u32(value) << shift) & mask) & kDspAssertIntBit) != 0) {
        u32 count = g_dspLastMailToDsp == 0 ? 2u : g_dspLastMailToDsp;
        if (count > kDspMaxCommandMails) {
            count = kDspMaxCommandMails;
        }
        g_dspCommandPhase = DspCommandPhase::AwaitingCommands;
        g_dspCommandsRemaining = count;
        g_dspCommandFirstMail = true;
    }

    // The DSP task boot handshake. src/dolphin/dsp/dsp_task.c's
    // __DSP_boot_task uploads the microcode as a fixed sequence of mails
    // ending with the marker 0x80F3D001 followed by the task's init vector;
    // on hardware the DSP then starts running that microcode, posts
    // 0xDCD10000 ("task started") and raises its interrupt.
    //
    // src/JSystem/osdsp_task.c's __DSPHandler is what receives that:
    // 0xDCD10000 sets DSP_prior_yield = 1 and calls the task's init_cb, which
    // for the audio task is DspHandShake - and DspHandShake waits for one
    // *more* mail, reads it, discards it, and calls Dsp_Running_Start().
    //
    // That is the whole reason this matters. DSPSendCommands2 opens with
    // `while (Dsp_Running_Check() == 0) ;`, and Dsp_Running_Check is just
    // `DSP_prior_yield == 1`. Measured: without this, the audio thread sat in
    // that loop for every one of 172,801 sampled slice boundaries.
    //
    // This is a simulation of the microcode's handshake, not a DSP. No
    // microcode runs, so no audio is produced - see the CPU->DSP mailbox
    // comment above.
    if ((addr & ~3u) == kRangeBase && addr == kCpuToDspMailboxLow) {
        constexpr u32 kBootInitVectorMarker = 0x80F3D001u;
        constexpr u32 kTaskStartedMail = 0xDCD10000u;
        g_dspLastMailToDsp = word;

        // A command stream in progress. The microcode acknowledges a completed
        // command by asking for the task's req_cb (0xDCD10004) and then
        // posting `JAS_DSP_PREFIX << 16 | id`, where id is the high half of
        // the first command mail - which is what syncDSP
        // (src/JSystem/JAudio/JASystem/JASAudioThread.cpp) reads and hands to
        // DspFinishWork, and what finally runs the caller's callback.
        //
        // Acknowledging unconditionally is safe: DspFinishWork compares the id
        // against the head of its own work queue and returns without doing
        // anything when it does not match, so a command that registered no
        // callback simply ignores this.
        if (g_dspCommandPhase == DspCommandPhase::AwaitingCommands) {
            if (g_dspCommandFirstMail) {
                g_dspCommandFirstMail = false;
                g_dspCommandWorkId = word >> 16;
            }
            if (g_dspCommandsRemaining > 0) {
                --g_dspCommandsRemaining;
            }
            if (g_dspCommandsRemaining == 0) {
                g_dspCommandPhase = DspCommandPhase::Idle;
                g_dspMailToCpu.push_back(kDspRequestMail);
                g_dspMailToCpu.push_back((kJasDspPrefix << 16) | (g_dspCommandWorkId & 0xFFFFu));
                g_shadow[kDspWordControlStatus] |= kDspInterruptStatusBit;
                interrupt::raise(interrupt::kCauseDsp);
                if (first_time_seeing("dsp-command-ack", addr, g_dspCommandWorkId)) {
                    Log.info("DSP command {:#06x} complete -> acknowledging with {:#010x}", g_dspCommandWorkId,
                        (kJasDspPrefix << 16) | (g_dspCommandWorkId & 0xFFFFu));
                }
            }
        }

        if (word == kBootInitVectorMarker) {
            g_dspBootAwaitingInitVector = true;
        } else if (g_dspBootAwaitingInitVector) {
            g_dspBootAwaitingInitVector = false;
            g_dspMailToCpu.push_back(kTaskStartedMail);
            // DspHandShake reads this one and throws it away without looking
            // at it, so its value carries no meaning; only its presence does.
            g_dspMailToCpu.push_back(0x80000000u);
            g_shadow[kDspWordControlStatus] |= kDspInterruptStatusBit;
            interrupt::raise(interrupt::kCauseDsp);
            Log.info("DSP task boot complete (init vector {:#010x}) -> posting {:#010x} and raising the DSP interrupt",
                word, kTaskStartedMail);
        }
    }

    // ar.c writes the DMA registers in a fixed order and finishes with
    // DSP_ARAM_DMA_SIZE_LO, so that write is the start signal. Running the
    // transfer synchronously here also satisfies __ARWaitForDMA(), which
    // spins while DSP_CONTROL_STATUS bit 9 (0x200) is set: the transfer is
    // already over by the time the guest can look, so that bit is never
    // set and the wait falls straight through.
    if (write_starts_aram_dma(addr, size)) {
        perform_aram_dma(cpu);
    }
}

// The REAL EXI (External Interface - memory card/GBA link/serial device
// bus) hardware register block, at its actual documented address -
// distinct from the mis-hypothesized 0xCC0050xx range above (that turned
// out to be the DSP interface instead, see that range's own comment).
// Reached for real once DVDInit's own translated body runs (verified:
// bridging DVDInit to Aurora's real no-op DVDInit() - matching how
// CARDInit bridges to a real implementation - stopped the "bootrom"
// OSReport loop it was stuck in, but its own caller then called it again
// in an immediate, unbroken retry loop, the same "no-op stub doesn't set
// a guest-side flag the caller polls for" lesson as EXIInit - so DVDInit
// isn't bridged either, letting its real body run and reach this).
//
// Register layout per channel (base 0xCC006800, 3 channels 0x14 bytes
// apart: 0xCC006800/6814/6828) is real, not guessed - Dolphin emulator's
// own EXI_Channel.h (Source/Core/Core/HW/EXI/EXI_Channel.h):
//   +0x00 Status:  bit 12 = EXT (device physically present, read-only)
//   +0x04 DMA address, +0x08 DMA length (not touched here)
//   +0x0C Control: bit 0 = TSTART (write 1 to start an immediate/DMA
//                  transfer; real hardware clears it back to 0 once the
//                  transfer completes - the same self-clearing-bit
//                  pattern already used for DSPCR above, now for a
//                  precisely-known real bit instead of a guessed one)
//   +0x10 Immediate data register
// A zero-initialized shadow already gives the right answer for EXT
// (clear = "nothing attached", exactly what a real, cardless/deviceless
// setup should report) without any special-casing - only TSTART needs
// the self-clear simulation, since nothing here has real EXI hardware to
// finish a transfer and clear it on its own.
constexpr u32 kExiChannelBase = 0xCC006800u;
constexpr u32 kExiChannelEnd = 0xCC006840u; // 3 channels x 0x14, rounded up
constexpr u32 kExiChannelStride = 0x14u;
constexpr u32 kExiControlOffset = 0x0Cu;
constexpr u32 kExiTStartBit = 1u << 0;
std::array<u32, (kExiChannelEnd - kExiChannelBase) / 4> g_exiShadow {};

bool is_exi_control_register(u32 wordAddr)
{
    return (wordAddr - kExiChannelBase) % kExiChannelStride == kExiControlOffset;
}

u64 exi_read(CPUState *cpu, u32 addr, u8 size)
{
    const u32 wordAddr = addr & ~3u;
    const size_t index = (wordAddr - kExiChannelBase) / 4;
    u32 word = index < g_exiShadow.size() ? g_exiShadow[index] : 0;
    if (is_exi_control_register(wordAddr)) {
        word &= ~kExiTStartBit;
    }
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u64 value = (u64(word) >> shift) & ((size == 4) ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1));
    if (first_time_seeing("exi-read", addr, value)) {
        Log.info("EXI read  {:#010x} (size={}) -> {:#x} [word {:#010x}] pc={:#010x}", addr, size, value, word, cpu->pc);
    }
    return value;
}

void exi_write(CPUState *cpu, u32 addr, u64 value, u8 size)
{
    const u32 wordAddr = addr & ~3u;
    const size_t index = (wordAddr - kExiChannelBase) / 4;
    if (index >= g_exiShadow.size()) {
        return;
    }
    u32 &word = g_exiShadow[index];
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u32 mask = (size == 4 ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1)) << shift;
    word = (word & ~mask) | ((u32(value) << shift) & mask);
    if (first_time_seeing("exi-write", addr, value)) {
        Log.info("EXI write {:#010x} (size={}) <- {:#x} [word now {:#010x}] pc={:#010x}", addr, size, value, word, cpu->pc);
    }
}

// The GameCube DI (Disc Interface) hardware register block - genuinely
// disc-hardware-related, unlike EXI/DSP above, reached once DVDInit's own
// translated body starts issuing real disc commands (writes to
// 0xCC006000/6004 observed right before it, in a real run, immediately
// preceding an OSReport("bootrom") retry loop - a disc/BS2-version
// diagnostic message, judging by the name).
//
// Layout (base 0xCC006000) per Dolphin emulator's own DVDInterface.h
// (Source/Core/Core/HW/DVD/DVDInterface.h) and the standard GC memory
// map: +0x00 DISR (status), +0x04 DICVR (cover - bit 0 = cover open,
// clear = closed, matching a zero shadow's default), +0x08/0x0C/0x10
// DICMDBUF0-2, +0x14 DIMAR, +0x18 DILENGTH, +0x1C DICR (control - bit 0
// TSTART, hardware-cleared on completion, the same self-clearing pattern
// as EXI/DSPCR above), +0x20 DIIMMBUF, +0x24 DICFG.
//
// DICFG is what the OSReport("bootrom") retry loop turned out to be
// waiting on - found by reading the real hardware address it polls
// (0xCC006024, from a real run's DI-read log lines) against Dolphin's own
// DVDInterface.cpp::ResetDrive(), not guessed: `m_DICFG.Hex = 0;
// m_DICFG.CONFIG = 1; // Disable bootrom descrambler` runs unconditionally
// for every boot (the Triforce-only `|= 8` right after it does not apply
// here). A zero-filled shadow answered 0 at this address forever, so the
// bootrom-descrambler check the game was polling for never passed.
constexpr u32 kDiBase = 0xCC006000u;
constexpr u32 kDiEnd = 0xCC006030u;
constexpr u32 kDiControlOffset = 0x1Cu;
constexpr u32 kDiTStartBit = 1u << 0;
constexpr u32 kDiConfigOffset = 0x24u;
constexpr u32 kDiConfigDisableBootromDescrambler = 1u << 0;
std::array<u32, (kDiEnd - kDiBase) / 4> g_diShadow {};

u64 di_read(CPUState *cpu, u32 addr, u8 size)
{
    const u32 wordAddr = addr & ~3u;
    const size_t index = (wordAddr - kDiBase) / 4;
    u32 word = index < g_diShadow.size() ? g_diShadow[index] : 0;
    if (wordAddr - kDiBase == kDiControlOffset) {
        word &= ~kDiTStartBit;
    }
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u64 value = (u64(word) >> shift) & ((size == 4) ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1));
    if (first_time_seeing("di-read", addr, value)) {
        Log.info("DI read  {:#010x} (size={}) -> {:#x} [word {:#010x}] pc={:#010x}", addr, size, value, word, cpu->pc);
    }
    return value;
}

// --- DI: the drive's DMA engine --------------------------------------------
//
// Everything above this point only shadowed the DI registers, which let the
// guest believe a transfer had finished while not one byte ever moved. That is
// not a missing nicety: the game reads *all* of its data off the disc, so it
// was running on zeros. Measured consequence: JAIData::initData looped forever
// because JAIGlobalParameter::getParamSeCategoryMax() returns
// mSeTable.mCategoryMax, read out of a sound-info file that never arrived, and
// the audio heap's bump pointer climbed by a constant amount every frame.
//
// The register programming below is taken from src/dolphin/dvd/dvdlow.c, which
// is the very code issuing it. __DIRegs is a u32*, so index 2 is 0xCC006008:
//
//     Read():              __DIRegs[2] = 0xa8000000;
//                          __DIRegs[3] = offset / 4;
//                          __DIRegs[4] = length;
//                          __DIRegs[5] = (u32)addr;
//                          __DIRegs[6] = length;
//                          __DIRegs[7] = 3;
//     DVDLowReadDiskID():  __DIRegs[2] = 0xa8000040;  (same shape, 0x20 bytes)
//     DVDLowInquiry():     __DIRegs[2] = 0x12000000;
//
// The first command the boot ever issues is the inquiry, which is why nothing
// downstream of it had ever run.
constexpr u32 kDiStatusOffset = 0x00u;
constexpr u32 kDiCmdBuf0Offset = 0x08u;
constexpr u32 kDiCmdBuf1Offset = 0x0Cu;
constexpr u32 kDiCmdBuf2Offset = 0x10u;
constexpr u32 kDiMemAddressOffset = 0x14u;
constexpr u32 kDiLengthOffset = 0x18u;

// DISR's transfer-complete status bit, and it is bit 4, not bit 2. The layout
// is not the one a datasheet summary suggests; it is the one
// __DVDInterruptHandler (src/dolphin/dvd/dvdlow.c) decodes:
//
//     mask = reg & 0x2a;                   // mask bits 1, 3, 5
//     intr = (reg & 0x54) & (mask << 1);   // status bits 2, 4, 6
//     if (intr & 0x40) cause |= 8;         // bit 6 -> break/cancel
//     if (intr & 0x10) cause |= 1;         // bit 4 -> transfer complete
//     if (intr & 4)    cause |= 2;         // bit 2 -> device error
//
// and cbForStateBusy (src/dolphin/dvd/dvd.c) treats `intType & 1` as the
// success path and `intType & 2` as the error path.
//
// Setting bit 2 therefore told the SDK every single successful transfer was a
// drive failure. Measured: the inquiry completed, and the very next command
// was DVDLowStopMotor - the drive being shut down as unusable, which is the
// error path's first act. The SDK's DI interrupt handler acknowledges by
// writing the bit back set, so it is write-one-to-clear.
constexpr u32 kDiStatusTransferComplete = 1u << 4;

// Aurora's DVDReadAbsAsyncPrio asserts a 32-byte aligned buffer and a length
// that is a multiple of 32 - the same constraints real DI DMA has. The
// bounce buffer is over-allocated so a 32-byte aligned pointer can be carved
// out of it regardless of what the allocator returned.
std::vector<u8> g_diBounce;

u8 *di_bounce_buffer(size_t length)
{
    g_diBounce.assign(length + 32u, 0);
    auto *raw = g_diBounce.data();
    const auto misalignment = reinterpret_cast<uintptr_t>(raw) & 31u;
    return misalignment == 0 ? raw : raw + (32u - misalignment);
}

void perform_di_command(CPUState *cpu)
{
    const u32 command = g_diShadow[kDiCmdBuf0Offset / 4];
    const u32 arg1 = g_diShadow[kDiCmdBuf1Offset / 4];
    const u32 memAddress = g_diShadow[kDiMemAddressOffset / 4];
    const u32 length = g_diShadow[kDiLengthOffset / 4];
    const u32 opcode = command >> 24;

    // DICR bit 1 selects DMA. Commands that do not set it move no memory at
    // all: DVDLowStopMotor writes `__DIRegs[7] = 1` and never touches DIMAR or
    // DILENGTH, so acting on their stale values wrote 32 bytes of zeros over
    // whatever the *previous* command's buffer had been. Measured doing
    // exactly that, to 0x803F9D20, before this check existed.
    constexpr u32 kDiDmaBit = 1u << 1;
    const bool isDma = (g_diShadow[kDiControlOffset / 4] & kDiDmaBit) != 0;

    // Aurora wants a 32-byte multiple; the guest already asks for those, but
    // rounding up costs nothing and keeps a short tail read legal.
    const u32 alignedLength = (length + 31u) & ~31u;

    if (!isDma) {
        // Nothing to transfer - just acknowledge below.
    } else if (opcode == 0xA8u && length > 0) {
        // DICMDBUF1 holds the disc offset in 4-byte units, so it must be
        // widened before scaling - a 1.1GB disc's offset/4 fits in 32 bits but
        // the byte offset is what overflows first on a bigger image.
        const u64 discOffset = static_cast<u64>(arg1) * 4ull;
        u8 *bounce = di_bounce_buffer(alignedLength);
        DVDCommandBlock block {};
        DVDReadAbsAsyncPrio(&block, bounce, static_cast<s32>(alignedLength), static_cast<s32>(discOffset), nullptr, 2);

        // DIMAR is a physical address on hardware; the guest hands over a
        // cached pointer, so the same masking the ARAM DMA above uses applies.
        // Copied a byte at a time deliberately: this is opaque disc data, not
        // words, and must not be byte-swapped on the way in.
        const u32 guestBase = (memAddress & 0x03FFFFFFu) | GC_RAM_BASE;
        for (u32 i = 0; i < length; ++i) {
            mem_write8(cpu, guestBase + i, bounce[i]);
        }
        if (first_time_seeing("di-read", command, discOffset)) {
            Log.info("DI read: cmd={:#010x} discOffset={:#x} length={:#x} -> {:#010x}", command, discOffset, length,
                guestBase);
        }
    } else if (length > 0) {
        // Every other command (inquiry, status, audio streaming...) is
        // completed with zeros rather than left hanging. That is explicitly
        // NOT an implementation of them: the guest gets a well-formed empty
        // answer so it stops waiting, and anything that actually depends on
        // the content will be wrong until the command is modelled for real.
        const u32 guestBase = (memAddress & 0x03FFFFFFu) | GC_RAM_BASE;
        for (u32 i = 0; i < length; ++i) {
            mem_write8(cpu, guestBase + i, 0);
        }
        if (first_time_seeing("di-unimplemented", command, length)) {
            Log.warn("DI command {:#010x} is not implemented - completing it with {:#x} zero bytes at {:#010x}",
                command, length, guestBase);
        }
    }

    // DILENGTH counts down as the DMA proceeds and reads back as bytes still
    // outstanding. cbForStateBusy does
    // `executing->transferredSize += executing->currTransferSize - __DIRegs[6];`
    // and re-issues the command whenever transferredSize has not reached the
    // requested length, so leaving it at its programmed value would make every
    // completed transfer look like it moved nothing.
    if (isDma) {
        g_diShadow[kDiLengthOffset / 4] = 0;
    }

    // A transfer only counts as finished when the drive says so: the SDK's
    // completion callbacks all run from the DI interrupt handler, so raising
    // it is what actually resumes the guest's DVD state machine.
    g_diShadow[kDiStatusOffset / 4] |= kDiStatusTransferComplete;
    interrupt::raise(interrupt::kCauseDi);
}

void di_write(CPUState *cpu, u32 addr, u64 value, u8 size)
{
    const u32 wordAddr = addr & ~3u;
    const size_t index = (wordAddr - kDiBase) / 4;
    if (index >= g_diShadow.size()) {
        return;
    }
    u32 &word = g_diShadow[index];
    const u32 oldWord = word;
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u32 mask = (size == 4 ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1)) << shift;
    word = (word & ~mask) | ((u32(value) << shift) & mask);
    if (first_time_seeing("di-write", addr, value)) {
        Log.info("DI write {:#010x} (size={}) <- {:#x} [word now {:#010x}] pc={:#010x}", addr, size, value, word, cpu->pc);
    }

    // DISR's status bits are write-one-to-clear, so they cannot ride the plain
    // shadow write. Once none is left outstanding the PI cause has to drop too,
    // or dispatch would re-enter the same interrupt forever.
    if (wordAddr - kDiBase == kDiStatusOffset && (mask & kDiStatusTransferComplete) != 0) {
        const u32 written = (u32(value) << shift) & mask;
        if ((written & kDiStatusTransferComplete) != 0) {
            word &= ~kDiStatusTransferComplete;
            interrupt::clear(interrupt::kCauseDi);
        } else {
            word |= (oldWord & kDiStatusTransferComplete);
        }
    }

    // DICR bit 0 (TSTART) is the go signal, written last - see dvdlow.c's
    // `__DIRegs[7] = 3;`.
    if (wordAddr - kDiBase == kDiControlOffset && (word & kDiTStartBit) != 0) {
        perform_di_command(cpu);
    }
}

// --- AI (Audio Interface) --------------------------------------------------
//
// src/dolphin/ai/ai.c drives this block through a `u32* __AIRegs` based at
// 0xCC006C00: index 0 is the control register, 1 the stream volume, 2 the
// free-running sample counter, 3 the interrupt timing. Every bit meaning
// below is read out of that file rather than assumed - `AIGetStreamPlayState`
// is literally `__AIRegs[0] & 1`, `AISetStreamSampleRate` stores
// AI_SAMPLERATE_32KHZ(0)/_48KHZ(1) at bit 1, the counter reset is written as
// `(__AIRegs[0] & ~0x20) | 0x20` (bit 5), and `AIGetDSPSampleRate` reads bit 6
// back inverted.
//
// __AI_SRC_INIT is what makes this range mandatory rather than nice to have:
// it does `temp0 = __AIRegs[2]; while (temp0 == __AIRegs[2]) {}` twice, once
// per stream rate, and times the gap with OSGetTime() to identify the part.
// With no handler the counter read as 0 forever - measured as a boot parked
// in __AI_SRC_INIT and 987MB of a single repeated warning line.
constexpr u32 kAiBase = 0xCC006C00u;
constexpr u32 kAiEnd = 0xCC006C10u;
constexpr u32 kAiControlOffset = 0x00u;
constexpr u32 kAiSampleCountOffset = 0x08u;
constexpr u32 kAiPlayStateBit = 1u << 0;
constexpr u32 kAiStreamRateBit = 1u << 1; // 0 = 32kHz, 1 = 48kHz
constexpr u32 kAiSampleCountResetBit = 1u << 5;

// The counter has to advance on the same clock OSGetTime() reads, or the
// interval __AI_SRC_INIT measures across it means nothing. OS_TIMER_CLOCK is
// the decomp's own OS_BUS_CLOCK / 4 (include/dolphin/os.h).
constexpr u64 kTimerClockHz = 40500000ull;

std::array<u32, (kAiEnd - kAiBase) / 4> g_aiShadow {};
u64 g_aiSampleCount = 0;
u64 g_aiCountedThroughTimebase = 0;

u32 ai_stream_rate_hz()
{
    return (g_aiShadow[kAiControlOffset / 4] & kAiStreamRateBit) ? 48000u : 32000u;
}

// Turns however much guest time has passed into whole samples, and advances
// the mark by only the ticks actually converted. Carrying the remainder that
// way is what keeps the counter phase-locked to the timebase: a poll that
// arrives late must not lose the fraction of a sample it straddled, or the
// measured interval between two ticks drifts away from the real period.
void ai_advance_sample_count(const CPUState *cpu)
{
    if ((g_aiShadow[kAiControlOffset / 4] & kAiPlayStateBit) == 0) {
        // Stopped: no samples, and no backlog to catch up on when it restarts.
        g_aiCountedThroughTimebase = cpu->timebase;
        return;
    }
    if (cpu->timebase <= g_aiCountedThroughTimebase) {
        return;
    }
    const u64 rate = ai_stream_rate_hz();
    const u64 samples = (cpu->timebase - g_aiCountedThroughTimebase) * rate / kTimerClockHz;
    if (samples == 0) {
        return;
    }
    g_aiSampleCount += samples;
    g_aiCountedThroughTimebase += samples * kTimerClockHz / rate;

    // Bounded trace of the interval the guest actually measures across a tick.
    // __AI_SRC_INIT compares it against bounds expressed in ticks
    // (OSNanosecondsToTicks(31524) and friends, src/dolphin/ai/ai.c), so this
    // prints ticks, in the same units, rather than something needing
    // conversion to compare.
    static u64 lastTickTimebase = 0;
    static unsigned traced = 0;
    if (traced < 40) {
        ++traced;
        Log.info("AI tick: count={} rate={}Hz timebase={} delta={} ticks pc={:#010x}", g_aiSampleCount, rate,
            cpu->timebase, cpu->timebase - lastTickTimebase, cpu->pc);
    }
    lastTickTimebase = cpu->timebase;
}

u64 ai_read(CPUState *cpu, u32 addr, u8 size)
{
    const u32 wordAddr = addr & ~3u;
    const u32 offset = wordAddr - kAiBase;
    u32 word = 0;
    if (offset == kAiSampleCountOffset) {
        ai_advance_sample_count(cpu);
        word = static_cast<u32>(g_aiSampleCount);
    } else {
        const size_t index = offset / 4;
        word = index < g_aiShadow.size() ? g_aiShadow[index] : 0;
    }
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u64 value = (u64(word) >> shift) & ((size == 4) ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1));
    // Deliberately not logging the sample counter: it is polled in a tight
    // loop by design, so "first time seeing this value" would still be a new
    // line on almost every read.
    if (offset != kAiSampleCountOffset && first_time_seeing("ai-read", addr, value)) {
        Log.info("AI read  {:#010x} (size={}) -> {:#x} [word {:#010x}] pc={:#010x}", addr, size, value, word, cpu->pc);
    }
    return value;
}

void ai_write(CPUState *cpu, u32 addr, u64 value, u8 size)
{
    const u32 wordAddr = addr & ~3u;
    const u32 offset = wordAddr - kAiBase;
    const size_t index = offset / 4;
    if (index >= g_aiShadow.size()) {
        return;
    }

    // Bring the counter up to date under the *old* control settings before the
    // write can change the rate or the play state, so the samples already
    // earned are counted at the rate they were produced at.
    ai_advance_sample_count(cpu);

    u32 &word = g_aiShadow[index];
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u32 mask = (size == 4 ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1)) << shift;
    word = (word & ~mask) | ((u32(value) << shift) & mask);

    if (offset == kAiControlOffset && (word & kAiSampleCountResetBit) != 0) {
        g_aiSampleCount = 0;
        g_aiCountedThroughTimebase = cpu->timebase;
        // Self-clearing on hardware: the guest writes it as a pulse and reads
        // the register back expecting it gone (`(__AIRegs[0] & ~0x20) | 0x20`
        // is a write, never a state it polls for).
        word &= ~kAiSampleCountResetBit;
    }

    if (first_time_seeing("ai-write", addr, value)) {
        Log.info("AI write {:#010x} (size={}) <- {:#x} [word now {:#010x}] pc={:#010x}", addr, size, value, word, cpu->pc);
    }
}

} // namespace

void install()
{
    g_shadow.fill(0);
    // Left permanently set rather than cleared on the "consuming" read a
    // real read-clear mailbox would have - tried that, and reading the
    // actual generated code (see the big comment above) confirmed it is
    // genuinely a two-loop, opposite-polarity handshake, but a real
    // run showed the two loops don't run in the static code's apparent
    // order (the "wait for clear" loop runs and consumes the response
    // *before* the "wait for set" loop checks it again), meaning a
    // second, real DSP-initiated message is expected between them - not
    // something derivable from the CPU-side code alone. Left simple and
    // permanently-ready rather than guessed at further.
    g_mailboxResponsePending = true;
    g_dspMailToCpu.clear();
    g_dspBootAwaitingInitVector = false;
    g_dspCommandPhase = DspCommandPhase::Idle;
    g_dspCommandsRemaining = 0;
    g_dspCommandWorkId = 0;
    g_dspCommandFirstMail = false;
    g_dspLastMailToDsp = 0;

    // ARAM starts out zeroed and, crucially, already reporting itself ready:
    // __ARChecksize's opening poll waits for DSP_ARAM_MODE bit 0 and never
    // gets it from a plain shadow.
    g_aram.assign(kAramSize, 0);
    g_shadow[kDspWordAramMode] |= kAramModeReadyBit;

    register_mmio_range({
        .base = kRangeBase,
        .end = kRangeEnd,
        .name = "DSP", // see this range's own comment - misnamed "EXI" originally
        .read = &read,
        .write = &write,
    });

    g_exiShadow.fill(0);
    register_mmio_range({
        .base = kExiChannelBase,
        .end = kExiChannelEnd,
        .name = "EXI",
        .read = &exi_read,
        .write = &exi_write,
    });

    g_diShadow.fill(0);
    g_diShadow[kDiConfigOffset / 4] = kDiConfigDisableBootromDescrambler;
    register_mmio_range({
        .base = kDiBase,
        .end = kDiEnd,
        .name = "DI",
        .read = &di_read,
        .write = &di_write,
    });

    g_aiShadow.fill(0);
    g_aiSampleCount = 0;
    g_aiCountedThroughTimebase = 0;
    register_mmio_range({
        .base = kAiBase,
        .end = kAiEnd,
        .name = "AI",
        .read = &ai_read,
        .write = &ai_write,
    });
}

namespace {

bool host_call_exi_not_present(CPUState *cpu, u32)
{
    cpu->gpr[3] = 0; // FALSE / not present, every probe-style EXI call
    return true;
}

} // namespace

void register_known_exi_calls(const NamedAddress *addresses, size_t count)
{
    std::vector<HostCallEntry> entries;
    for (size_t i = 0; i < count; ++i) {
        const char *name = addresses[i].name;
        HostCallFn fn = nullptr;
        // EXIInit is deliberately NOT bridged: verified with a real run
        // that stubbing it (as a no-op, matching Finding Set 1's original
        // proposal) causes an infinite loop calling it over and over -
        // real EXIInit almost certainly writes an "initialized" flag or
        // per-channel state into guest memory that other guest code polls
        // afterward, and a host_call stub with no guest-memory side effect
        // can never satisfy that. Since EXIInit's own translated body is
        // fully present (dolrecomp translated the whole DOL), it's safer
        // to let it run for real than to guess at what guest state it's
        // supposed to set - genuinely unrecognized hardware accesses
        // inside its own body will surface as their own, more specific,
        // diagnosable event instead.
        if (std::strcmp(name, "EXIProbe") == 0 ||
            std::strcmp(name, "EXIProbeEx") == 0 ||
            std::strcmp(name, "EXIGetState") == 0 ||
            std::strcmp(name, "EXIAttach") == 0) {
            fn = &host_call_exi_not_present;
        }
        if (fn) {
            entries.push_back({ addresses[i].address, name, fn });
        }
        // Everything else in the EXI family (EXIImm, EXIGetID, EXIDma...)
        // is deliberately left unregistered until a real call site is
        // observed - same discipline as PADRead/PADInit
        // (include/port/recomp_dolphin_sdk.h).
    }
    if (!entries.empty()) {
        register_host_calls(entries.data(), entries.size());
    }
}

} // namespace sms::recomp::exi
