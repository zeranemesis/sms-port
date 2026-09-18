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
// game makes to an address with no registered trampoline yet - which, once
// real game code actually runs (not just the self-tests), is many times
// per frame at the same handful of addresses. Logging every hit rather
// than every distinct address filled a 1.4GB log file in under two minutes
// running against a real GMSP01 dump. Each address is still worth exactly
// one warning - that's what tells you which HostCallEntry to add next -
// just not one per occurrence.
std::unordered_set<u32> &already_warned()
{
    static std::unordered_set<u32> instance;
    return instance;
}

bool dispatch(CPUState *cpu, u32 address)
{
    const auto it = table().find(address);
    if (it == table().end()) {
        if (already_warned().insert(address).second) {
            Log.warn("unresolved host call to {:#010x} from pc={:#010x} - "
                     "needs a HostCallEntry once the real MAP address is known",
                address, cpu->pc);
        }
        return false;
    }
    return it->second.fn(cpu, address);
}

} // namespace

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

u64 mmio_read(CPUState *cpu, u32 addr, u8 size)
{
    const MmioRangeHandler *handler = find_range(addr);
    if (!handler || !handler->read) {
        Log.warn("unmapped external read at {:#010x} (size={}) from pc={:#010x}", addr, size, cpu->pc);
        return 0;
    }
    return handler->read(cpu, addr, size);
}

void mmio_write(CPUState *cpu, u32 addr, u64 value, u8 size)
{
    const MmioRangeHandler *handler = find_range(addr);
    if (!handler || !handler->write) {
        Log.warn("unmapped external write at {:#010x} (size={} value={:#x}) from pc={:#010x}", addr, size, value, cpu->pc);
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
