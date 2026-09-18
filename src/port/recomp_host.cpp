#include "port/recomp_host.h"

#include "aurora/lib/logging.hpp"

#include <unordered_map>

namespace sms::recomp {
namespace {

aurora::Module Log("sms::recomp");

std::unordered_map<u32, HostCallEntry> &table()
{
    static std::unordered_map<u32, HostCallEntry> instance;
    return instance;
}

bool dispatch(CPUState *cpu, u32 address)
{
    const auto it = table().find(address);
    if (it == table().end()) {
        Log.warn("unresolved host call to {:#010x} from pc={:#010x} - "
                 "needs a HostCallEntry once the real MAP address is known",
            address, cpu->pc);
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

} // namespace sms::recomp
