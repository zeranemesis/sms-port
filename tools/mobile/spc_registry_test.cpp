// Standalone test: no game, assets, window, graphics, emulator or saves.
// Define SMS_SPC_REGISTRY_TEST_NO_MAIN to call the self-test from a JNI probe.
#include "../../platform/mobile/spc_native_registry.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace {
struct GenericInterp {
    uint32_t count;
    uint32_t last_argc;
    GenericInterp() : count(0), last_argc(0) {}
};
struct TypedInterp : GenericInterp {
    uint32_t effect;
    TypedInterp() : effect(0) {}
};

void generic_callback(GenericInterp* interp, uint32_t argc)
{
    ++interp->count;
    interp->last_argc = argc;
}
void typed_callback(TypedInterp* interp, uint32_t argc)
{
    ++interp->count;
    interp->last_argc = argc;
    interp->effect += argc * 7;
}
void reentrant_callback(GenericInterp* interp, uint32_t argc)
{
    const uint32_t nested = port_spc_register(&generic_callback);
    port_spc_call_checked(nested, interp, argc + 1);
}
// A different type and invoker must not share a token even if the linker folds
// identical machine code; representation + typed invoker form the identity.
void another_typed_callback(TypedInterp* interp, uint32_t argc)
{
    interp->effect += argc;
}

bool require(bool value, const char* message)
{
    if (!value)
        std::fprintf(stderr, "FAIL SPC registry: %s\n", message);
    return value;
}
} // namespace

int port_spc_registry_self_test(bool require_high_pointer)
{
    // This address is diagnostic only; it is never used to register/dispatch.
    const uintptr_t address = reinterpret_cast<uintptr_t>(&generic_callback);
    const bool high = sizeof(uintptr_t) > 4 && (address >> 16 >> 16) != 0;
    std::printf("SPC callback address=0x%llx high=%d\n",
                static_cast<unsigned long long>(address), int(high));
    if (!require(!require_high_pointer || high, "callback must be above 4 GiB"))
        return 1;

    void (*null_callback)(GenericInterp*, uint32_t) = 0;
    GenericInterp generic;
    TypedInterp typed;
    if (!require(port_spc_register(null_callback) == 0, "null callback reserves zero") ||
        !require(!port_spc_invoke(0, &generic, 12), "zero token rejected") ||
        !require(!port_spc_invoke(UINT32_MAX, &generic, 12), "unknown token rejected") ||
        !require(generic.count == 0, "invalid tokens have no effects"))
        return 1;

    const uint32_t first = port_spc_register(&generic_callback);
    const uint32_t second = port_spc_register(&typed_callback);
    const uint32_t third = port_spc_register(&another_typed_callback);
    if (!require(first && second && third, "valid callbacks receive nonzero tokens") ||
        !require(first != second && second != third && first != third, "distinct callbacks/types") ||
        !require(port_spc_register(&generic_callback) == first, "generic deduplication") ||
        !require(port_spc_register(&typed_callback) == second, "typed deduplication") ||
        !require(port_spc_invoke(first, &generic, 3), "generic invocation") ||
        !require(generic.count == 1 && generic.last_argc == 3, "generic argument and effect") ||
        !require(port_spc_invoke(second, &typed, 9), "typed invocation") ||
        !require(typed.count == 1 && typed.last_argc == 9 && typed.effect == 63, "typed argument and effect"))
        return 1;

    port_spc_call_checked(second, &typed, 0);
    if (!require(typed.count == 2 && typed.last_argc == 0 && typed.effect == 63, "checked valid invocation"))
        return 1;
    const uint32_t reentrant = port_spc_register(&reentrant_callback);
    if (!require(port_spc_invoke(reentrant, &generic, 25), "reentrant invocation") ||
        !require(generic.count == 2 && generic.last_argc == 26, "nested register/invoke without deadlock"))
        return 1;

    std::atomic<bool> success(true);
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.push_back(std::thread([&success, first]() {
            GenericInterp own;
            for (uint32_t n = 0; n < 1000; ++n) {
                if (port_spc_register(&generic_callback) != first ||
                    !port_spc_invoke(first, &own, n))
                    success.store(false);
            }
            if (own.count != 1000 || own.last_argc != 999)
                success.store(false);
        }));
    }
    for (size_t i = 0; i < threads.size(); ++i)
        threads[i].join();
    if (!require(success.load(), "concurrent registration/deduplication/invocation"))
        return 1;

    std::puts("PASS SPC registry: native high address, generic/typed, null/unknown, checked, reentrant, dedup, threads");
    return 0;
}

#ifndef SMS_SPC_REGISTRY_TEST_NO_MAIN
int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--abort-invalid") == 0) {
        port_spc_call_checked(0, 0, 0);
        return 2; // Unreachable: checked invalid must terminate the process.
    }
    const bool high = argc == 2 && std::strcmp(argv[1], "--require-high") == 0;
    return port_spc_registry_self_test(high);
}
#endif
