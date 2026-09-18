#include "port/recomp_exi.h"
#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"

#include <array>
#include <cstring>
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
constexpr u16 kMailboxValidBit = 0x8000u;
bool g_mailboxResponsePending = false;

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
    word &= ~kSelfClearingResetBit;

    const bool touchesMailboxWord = (addr & ~3u) == kDspToCpuMailboxHigh;
    if (touchesMailboxWord && g_mailboxResponsePending) {
        word |= (u32(kMailboxValidBit) << 16);
    }

    // Sub-word reads pull the requested bytes out of the containing
    // big-endian 32-bit word, matching how a real register would be
    // byte/halfword-addressable within its word.
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u64 value = (u64(word) >> shift) & ((size == 4) ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1));
    Log.info("read  {:#010x} (size={}) -> {:#x} [word {:#010x}] pc={:#010x}", addr, size, value, word, cpu->pc);
    return value;
}

void write(CPUState *cpu, u32 addr, u64 value, u8 size)
{
    const size_t index = word_index(addr & ~3u);
    if (index >= g_shadow.size()) {
        return;
    }
    u32 &word = g_shadow[index];
    const u32 shift = (4 - size - (addr & 3u)) * 8;
    const u32 mask = (size == 4 ? 0xFFFFFFFFu : ((1u << (size * 8)) - 1)) << shift;
    word = (word & ~mask) | ((u32(value) << shift) & mask);
    Log.info("write {:#010x} (size={}) <- {:#x} [word now {:#010x}] pc={:#010x}", addr, size, value, word, cpu->pc);

    // Writing the CPU->DSP mailbox's high half is the "send a command"
    // step - simulate the DSP responding instantly (nothing here has a
    // real DSP core to respond for real, or to know what it should say -
    // see the comment above g_mailboxResponsePending).
    if ((addr & ~3u) == kRangeBase) {
        g_mailboxResponsePending = true;
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
    register_mmio_range({
        .base = kRangeBase,
        .end = kRangeEnd,
        .name = "EXI",
        .read = &read,
        .write = &write,
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
