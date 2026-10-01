#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sms::recomp {
namespace {

aurora::Module Log("sms::recomp");

std::unordered_map<u32, HostCallEntry> &table()
{
    static std::unordered_map<u32, HostCallEntry> instance;
    return instance;
}

// A miss here fires on every cross-chunk branch/host call the recompiled
// game makes to an address with no registered trampoline. Logging every hit
// filled a 1.4GB log in under two minutes; deduplicating to one line per
// distinct address then still produced 16,050 lines - 2.9MB - at boot, and
// buried the handful of boot messages that actually mean something.
//
// Deduping was the wrong shape of fix, and the reason is worth recording.
// dispatch() returning false is not a failure: it is exactly the signal
// dr_cpu uses to run the guest's *own* body for that address, so an
// unbridged function keeps working. Measured both ways in this port - the
// "bootrom" OSReport loop, and EXIInit, where a no-op stub that did not
// maintain the guest-side flag its caller polls for turned a working
// function into an infinite loop. A bridge is only *needed* for a call that
// has to reach hardware Aurora owns; every other address is a bookkeeping
// gap, not a defect, and reporting it in the same voice as a real defect is
// what made the log unusable.
//
// So: the first few distinct addresses get a line, because each one does
// say "here is a call to consider bridging", and the rest are reported as a
// single rolling count. The count is the part that stayed worth having, and
// deduplicating alone kept it hidden behind the noise it was meant to
// replace.

// Individual lines stop here. Twenty is enough to recognise a pattern
// (all Dolphin SDK, all one subsystem, all one chunk boundary) and few
// enough that the summary below stays the thing the eye lands on.
constexpr size_t kMaxIndividuallyLoggedMisses = 20;

// The distinct-address set is capped for the same reason the unmapped-MMIO
// one is: past this point a growing number is no longer "calls we could
// bridge" but a pointer running away, and the cap is what says so rather
// than letting a 2.9MB log grow into the next one.
constexpr size_t kMaxDistinctMissedAddresses = 4096;

// 40.5MHz timebase (486MHz core / 12, see kCoreCyclesPerTimebaseTick below),
// so this is one second of guest time. The summary reports a rate, and a
// rate sampled faster than a human reads is just the flood again.
constexpr u64 kMissReportIntervalTimebase = 40500000ull;

void report_missed_host_call(CPUState *cpu, u32 address)
{
    static std::unordered_set<u32> distinct;
    static std::unordered_set<u32> logged;
    static u64 calls = 0;
    static u64 lastReportTimebase = 0;
    static bool capReported = false;

    ++calls;
    const bool isNew = distinct.size() < kMaxDistinctMissedAddresses
        && distinct.insert(address).second;

    if (isNew && logged.size() < kMaxIndividuallyLoggedMisses) {
        logged.insert(address);
        Log.info("unresolved host call to {:#010x} from pc={:#010x} - not a failure, the guest "
                 "body runs; a HostCallEntry is only needed if this call must reach hardware",
            address, cpu->pc);
        return;
    }
    if (isNew && !capReported) {
        capReported = true;
        Log.info("more than {} distinct unresolved host call addresses seen - per-address lines have "
                 "stopped, the rolling count below is the real one",
            kMaxIndividuallyLoggedMisses);
    }

    if (cpu->timebase < lastReportTimebase + kMissReportIntervalTimebase) {
        return;
    }
    lastReportTimebase = cpu->timebase;
    Log.info("unresolved host calls in the last second: calls={} distinct seen={}{}",
        calls,
        distinct.size(),
        distinct.size() >= kMaxDistinctMissedAddresses ? " (capped, not counting further)" : "");
    calls = 0;
}

bool dispatch(CPUState *cpu, u32 address)
{
    const auto it = table().find(address);
    if (it == table().end()) {
        report_missed_host_call(cpu, address);
        return false;
    }
    if (!it->second.fn(cpu, address)) {
        return false;
    }
    // A bridge stands in for a whole guest subroutine, so it has to finish
    // the way that subroutine would: with the `blr` that puts the return
    // address back in pc. Nothing else does this for us - generated.h's
    // dolrecomp_call() sets `ctx->pc = address` on the way in and then
    // returns 1 as soon as ppc_host_call() reports the call handled, and
    // dolrecomp_run_blocks() re-reads ctx->pc for the very next iteration:
    //
    //     while (blocks < max_blocks) {
    //         if (!dolrecomp_call(ctx, ctx->pc)) return 0;
    //         blocks++;
    //     }
    //
    // so leaving pc pointing at the bridged address re-enters the same
    // bridge forever, burning the whole per-frame block budget without
    // advancing a single guest instruction. That is exactly what the
    // long-standing "DVDInit loops calling OSReport(\"bootrom\")" symptom
    // was: not a retry loop in the game at all, but this dispatcher
    // spinning on its own OSReport trampoline, with the repeat messages
    // invisible because they dedupe.
    cpu->pc = cpu->lr;

    // Charge the guest for the subroutine this bridge stood in for.
    //
    // Not a timing model - the real function's cost is unknown and this does
    // not pretend to estimate it. It is a forward-progress guarantee. A slice
    // ends when downcount is spent, and a bridge that returns without touching
    // downcount cannot end one; a loop made of bridged calls therefore runs
    // until the block cap, and interrupt delivery, which only happens between
    // slices, starves. Measured: removing the block cap stalled the game at
    // NLOGO with VI, DSP and DI all pending and unacknowledged, and the callees
    // that spend no cycles are precisely these bridges - OSReport, PADInit,
    // PADRead, PADSetAnalogMode, PADControlMotor, GXSetArray.
    //
    // 100 cycles is about 0.2us at 486MHz and 1/81,000th of a frame, so it
    // cannot distort the clock at any plausible call rate, while still
    // guaranteeing a slice made entirely of bridges ends after at most ~1,265
    // of them instead of never.
    constexpr s64 kBridgedCallCycles = 100;
    cpu->downcount -= kBridgedCallCycles;
    return true;
}

// The guest clock's state. Kept here rather than in CPUState so no dr_cpu
// patch is needed: the baseline is simply the downcount value the last
// accounting ran against, and anything below it has been executed since.
// See recomp_host.h for why guest time is derived from cycles at all.
s64 g_timebaseBaselineDowncount = 0;
s64 g_timebaseCycleRemainder = 0;

// 486MHz core / 40.5MHz OS_TIMER_CLOCK. Both constants are the decomp's own
// (include/dolphin/os.h: OS_BUS_CLOCK 162,000,000, timer = bus / 4, core =
// bus * 3), not round numbers chosen here.
constexpr s64 kCoreCyclesPerTimebaseTick = 12;

} // namespace

void sync_guest_timebase(CPUState *cpu)
{
    const s64 consumed = g_timebaseBaselineDowncount - cpu->downcount;
    if (consumed <= 0) {
        // Not execution: a refill or a restored CPUState raised the
        // downcount. rebase_guest_timebase() is what the caller owes us
        // there; silently re-baselining here would hide a missing call.
        return;
    }
    const s64 total = consumed + g_timebaseCycleRemainder;
    cpu->timebase += static_cast<u64>(total / kCoreCyclesPerTimebaseTick);
    g_timebaseCycleRemainder = total % kCoreCyclesPerTimebaseTick;
    g_timebaseBaselineDowncount = cpu->downcount;
}

void rebase_guest_timebase(CPUState *cpu)
{
    g_timebaseBaselineDowncount = cpu->downcount;
}

void register_host_calls(const HostCallEntry *entries, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        table()[entries[i].address] = entries[i];
    }
}

void install_host_calls(CPUState *cpu)
{
    cpu->host_call = &dispatch;
}

std::string read_guest_cstring(CPUState *cpu, u32 addr, size_t maxLen)
{
    std::string out;
    for (size_t i = 0; i < maxLen; ++i) {
        const u8 c = mem_read8(cpu, addr + static_cast<u32>(i));
        if (c == 0) {
            break;
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

namespace {

std::vector<MmioRangeHandler> &mmio_ranges()
{
    static std::vector<MmioRangeHandler> instance;
    return instance;
}

bool ranges_overlap(u32 aBase, u32 aEnd, u32 bBase, u32 bEnd)
{
    return aBase < bEnd && bBase < aEnd;
}

const MmioRangeHandler *find_range(u32 addr)
{
    for (const auto &handler : mmio_ranges()) {
        if (addr >= handler.base && addr < handler.end) {
            return &handler;
        }
    }
    return nullptr;
}

// An unmapped register is very often one the guest polls in a spin loop, which
// makes "log every access" a log bomb rather than a diagnostic. Measured: a
// 45-second run produced 987 MB, essentially all of it one line repeated -
// `unmapped external read at 0xcc006c08` from a single pc, the AI sample
// counter the guest waits on. The same class of runaway had already been fixed
// three times elsewhere in this port (host-call misses, OSReport, the DSP/EXI/
// DI registers), each time by deduplicating rather than by silencing, so do
// the same here: the first access to a given address still says everything
// needed to go and model that register, and the repeats say nothing new.
//
// Keyed on the address alone, not on (address, pc): a register is modelled or
// not, and a second caller polling the same unmapped register is the same
// defect.
//
// Deduplication alone is not enough, and assuming it was cost another 377MB
// log. It bounds the "tight poll of one register" case it was written for, but
// a wild pointer writes to a different address every time, so every write is a
// first. Measured: a run walking 0xA82F9508 upwards in 4-byte steps, one new
// warning line each. So the number of *distinct* addresses is capped too: past
// that point the port is not learning about one more unmodelled register, it
// is watching memory corruption, and the cap is what says so.
constexpr size_t kMaxDistinctUnmappedAddresses = 256;

bool first_unmapped_access(u32 addr)
{
    static std::unordered_set<u32> seen;
    static bool capReported = false;
    if (seen.size() >= kMaxDistinctUnmappedAddresses) {
        if (!capReported) {
            capReported = true;
            Log.warn("more than {} distinct unmapped external addresses have been touched - this is no longer a "
                     "missing register but a pointer running away; no further unmapped accesses will be logged",
                kMaxDistinctUnmappedAddresses);
        }
        return false;
    }
    return seen.insert(addr).second;
}

u64 mmio_read(CPUState *cpu, u32 addr, u8 size)
{
    // A hardware register read is the guest observing the world, so this is
    // the moment its clock has to be current - see sync_guest_timebase's
    // comment in recomp_host.h.
    sync_guest_timebase(cpu);
    const MmioRangeHandler *handler = find_range(addr);
    if (!handler || !handler->read) {
        if (first_unmapped_access(addr)) {
            Log.warn("unmapped external read at {:#010x} (size={}) from pc={:#010x} - reads as 0, further accesses to this address are not logged",
                addr, size, cpu->pc);
        }
        return 0;
    }
    return handler->read(cpu, addr, size);
}

void mmio_write(CPUState *cpu, u32 addr, u64 value, u8 size)
{
    sync_guest_timebase(cpu);
    const MmioRangeHandler *handler = find_range(addr);
    if (!handler || !handler->write) {
        if (first_unmapped_access(addr)) {
            Log.warn("unmapped external write at {:#010x} (size={} value={:#x}) from pc={:#010x} - discarded, further accesses to this address are not logged",
                addr, size, value, cpu->pc);
        }
        return;
    }
    handler->write(cpu, addr, value, size);
}

} // namespace

void register_mmio_range(const MmioRangeHandler &handler)
{
    for (const auto &existing : mmio_ranges()) {
        if (ranges_overlap(handler.base, handler.end, existing.base, existing.end)) {
            Log.error("register_mmio_range: '{}' [{:#010x}, {:#010x}) overlaps already-registered '{}' [{:#010x}, {:#010x}) - not registered",
                handler.name, handler.base, handler.end, existing.name, existing.base, existing.end);
            return;
        }
    }
    mmio_ranges().push_back(handler);
}

void install_external_memory(CPUState *cpu)
{
    cpu->external_read = &mmio_read;
    cpu->external_write = &mmio_write;
}

bool run_self_test()
{
    CPUState cpu {};
    if (!cpu_init(&cpu)) {
        Log.error("self-test: cpu_init failed");
        return false;
    }
    install_host_calls(&cpu);

    // Miss path: nothing is registered for this address yet, so the
    // dispatcher must log it and report false rather than crash.
    cpu.pc = 0x80003000u;
    if (cpu.host_call(&cpu, 0x80100000u)) {
        Log.error("self-test: dispatch should have missed an unregistered address");
        cpu_free(&cpu);
        return false;
    }

    // Hit path: a registered entry must run and see the CPUState the
    // dispatcher was called with, including the register a real Dolphin SDK
    // trampoline would read its first argument from.
    static bool called = false;
    called = false;
    const HostCallEntry entry {
        .address = 0x80100000u,
        .name = "TestHostCall",
        .fn =
            [](CPUState *ctx, u32) {
                called = ctx->gpr[3] == 42;
                ctx->gpr[3] = 1;
                return true;
            },
    };
    register_host_calls(&entry, 1);

    cpu.gpr[3] = 42;
    const bool dispatched = cpu.host_call(&cpu, 0x80100000u);
    cpu_free(&cpu);

    if (!dispatched || !called) {
        Log.error("self-test: registered host call was not dispatched correctly");
        return false;
    }

    Log.info("self-test: host-call dispatch table hit and miss paths both work");
    return true;
}

bool run_mmio_self_test()
{
    CPUState cpu {};
    if (!cpu_init(&cpu)) {
        Log.error("mmio self-test: cpu_init failed");
        return false;
    }
    install_external_memory(&cpu);

    // Miss path: an address outside every registered range reads as zero
    // and drops writes, exactly like dr_cpu's own unmapped-address fallback
    // did before this table existed.
    const u64 missRead = cpu.external_read(&cpu, 0xDEAD0000u, 4);
    if (missRead != 0) {
        Log.error("mmio self-test: miss read should be 0, got {:#x}", missRead);
        cpu_free(&cpu);
        return false;
    }
    cpu.external_write(&cpu, 0xDEAD0000u, 0x12345678u, 4); // must not crash

    // Hit path: a registered range must see the CPUState, address, and
    // size the dispatcher was called with.
    static u64 lastWritten = 0;
    static u8 lastWriteSize = 0;
    const MmioRangeHandler handler {
        .base = 0xCC000000u,
        .end = 0xCC000010u,
        .name = "TestMmioRange",
        .read = [](CPUState *, u32 addr, u8) -> u64 { return addr == 0xCC000004u ? 0x99u : 0u; },
        .write = [](CPUState *, u32, u64 value, u8 size) {
            lastWritten = value;
            lastWriteSize = size;
        },
    };
    register_mmio_range(handler);

    const u64 hitRead = cpu.external_read(&cpu, 0xCC000004u, 4);
    cpu.external_write(&cpu, 0xCC000008u, 0xABCDu, 2);
    cpu_free(&cpu);

    if (hitRead != 0x99u) {
        Log.error("mmio self-test: expected hit read 0x99, got {:#x}", hitRead);
        return false;
    }
    if (lastWritten != 0xABCDu || lastWriteSize != 2) {
        Log.error("mmio self-test: expected write 0xabcd size 2, got {:#x} size {}", lastWritten, lastWriteSize);
        return false;
    }

    Log.info("mmio self-test: external_read/external_write range dispatch hit and miss paths both work");
    return true;
}

} // namespace sms::recomp
