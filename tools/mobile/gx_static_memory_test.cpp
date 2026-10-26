// Standalone immutable GX staging tests. No game/assets/graphics or ADB.
#include "../../platform/mobile/gx_static_memory.h"
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {
const size_t window_bytes = 256 * 1024;
unsigned char source_a[256];
unsigned char source_b[256];
unsigned char source_c[256];
unsigned char sphere[0x10cc];
struct Allocator {
    unsigned char* mem1 = NULL;
    size_t cursor = 256;
    unsigned calls = 0;
    int mode = 0;
    void* reused = NULL;
    const void* pending_source = NULL;
    size_t pending_bytes = 0;
    bool reentry_checked = false;
} allocator;

bool require(bool condition, const char* label)
{
    if (!condition)
        std::fprintf(stderr, "FAIL GX static memory: %s\n", label);
    return condition;
}
void* allocate(size_t bytes, size_t alignment, void* context)
{
    Allocator* state = static_cast<Allocator*>(context);
    ++state->calls;
    if (alignment != 32)
        return NULL;
    if (state->pending_source) {
        // This must not deadlock or recursively allocate while the outer call
        // is waiting for its rootheap allocation to finish.
        state->reentry_checked =
            port_gx_static_configure(state->mem1, window_bytes, allocate, state) &&
            !port_gx_static_resolve(state->pending_source, state->pending_bytes) &&
            !port_gx_static_register(state->pending_source, state->pending_bytes);
    }
    if (state->mode == 1) return NULL;
    if (state->mode == 2) return reinterpret_cast<void*>(uintptr_t(0x40000000u));
    if (state->mode == 3) return state->mem1 + 3;
    if (state->mode == 4) return state->reused;
    if (state->mode == 5) return state->mem1 + window_bytes - 32;
    if (state->mode == 6) throw 17;
    const size_t at = (state->cursor + 31) & ~size_t(31);
    if (at > window_bytes || bytes > window_bytes - at)
        return NULL;
    state->cursor = at + bytes;
    return state->mem1 + at;
}
void* map_window()
{
    void* desired = reinterpret_cast<void*>(uintptr_t(0x80000000u));
#ifdef _WIN32
    return VirtualAlloc(desired, window_bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    // A hint only, then verify. Never replace another mapping in a JNI process.
    void* actual = mmap(desired, window_bytes, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (actual != desired) {
        if (actual != MAP_FAILED) munmap(actual, window_bytes);
        return NULL;
    }
    return actual;
#endif
}
bool round_trip(const void* pointer)
{
    const uintptr_t p = reinterpret_cast<uintptr_t>(pointer);
    const uintptr_t base = reinterpret_cast<uintptr_t>(allocator.mem1);
    const uint32_t phys = static_cast<uint32_t>(p - base);
    return phys != 0 && base + (phys & 0x3fffffffu) == p;
}
} // namespace

int port_gx_static_memory_self_test()
{
    for (size_t i = 0; i < sizeof(source_a); ++i) {
        source_a[i] = static_cast<unsigned char>(i);
        source_b[i] = static_cast<unsigned char>(i ^ 0xa5);
        source_c[i] = static_cast<unsigned char>(i ^ 0x5a);
    }
    for (size_t i = 0; i < sizeof(sphere); ++i)
        sphere[i] = static_cast<unsigned char>(i * 37);
    const uintptr_t source_address = reinterpret_cast<uintptr_t>(source_a);
    if (!require(sizeof(uintptr_t) > 4 && (source_address >> 16 >> 16) != 0, "actual source above 4 GiB") ||
        !require(!port_gx_static_register(source_a, 64), "register before configure") ||
        !require(!port_gx_static_resolve(source_a, 64), "resolve before configure") ||
        !require(!port_gx_static_configure(reinterpret_cast<void*>(uintptr_t(0x40000000u)), window_bytes,
                                         allocate, &allocator), "low pool is not MEM1") ||
        !require(!port_gx_static_configure(reinterpret_cast<void*>(uintptr_t(0x80000000u)), 32,
                                         allocate, &allocator), "empty usable MEM1 rejected"))
        return 1;
    allocator.mem1 = static_cast<unsigned char*>(map_window());
    if (!require(allocator.mem1 != NULL, "MEM1 mapping available"))
        return 1;
    if (!require(port_gx_static_configure(allocator.mem1, window_bytes, allocate, &allocator), "configure") ||
        !require(!port_gx_static_configure(allocator.mem1, window_bytes - 32, allocate, &allocator), "configuration cannot change") ||
        !require(!port_gx_static_resolve(source_c, 1), "unknown high range rejected") ||
        !require(!port_gx_static_register(NULL, 32), "null rejected") ||
        !require(!port_gx_static_register(source_a, 0), "empty source rejected") ||
        !require(!port_gx_static_register(reinterpret_cast<void*>(UINTPTR_MAX - 15), 32), "overflow rejected"))
        return 1;
    allocator.pending_source = source_a + 32;
    allocator.pending_bytes = 64;
    const void* staged_a = port_gx_static_register(source_a + 32, 64);
    allocator.pending_source = NULL;
    if (!require(staged_a && !(reinterpret_cast<uintptr_t>(staged_a) & 31), "aligned MEM1 staging") ||
        !require(allocator.reentry_checked, "allocator runs outside lock with pending source unpublished") ||
        !require(std::memcmp(staged_a, source_a + 32, 64) == 0, "source bytes preserved") ||
        !require(round_trip(staged_a), "physical FIFO round-trip") ||
        !require(!round_trip(reinterpret_cast<void*>(uintptr_t(0x40000000u))), "outside-MEM1 low pool fails FIFO round-trip"))
        return 1;
    const unsigned calls = allocator.calls;
    unsigned char* writable = const_cast<unsigned char*>(static_cast<const unsigned char*>(staged_a));
    writable[0] ^= 0xff;
    const unsigned char marker = writable[0];
    for (int i = 0; i < 100; ++i) {
        if (!require(port_gx_static_register(source_a + 32, 64) == staged_a, "stable deduplicated pointer"))
            return 1;
    }
    if (!require(allocator.calls == calls && writable[0] == marker, "no repeated allocation or copy"))
        return 1;
    writable[0] = source_a[32];
    if (!require(port_gx_static_resolve(source_a + 40, 8) == writable + 8, "source subrange resolves with offset") ||
        !require(port_gx_static_resolve(writable + 8, 8) == writable + 8, "copy subrange resolves") ||
        !require(port_gx_static_original(writable + 8, 8) == source_a + 40, "reverse identity with offset") ||
        !require(port_gx_static_original(source_a + 40, 8) == source_a + 40, "original identity") ||
        !require(!port_gx_static_register(source_a + 48, 64), "overlapping source registration rejected") ||
        !require(!port_gx_static_register(source_a + 32, 32), "contained new registration rejected; use resolve") ||
        !require(!port_gx_static_resolve(source_a + 24, 16), "partial source overlap rejected") ||
        !require(!port_gx_static_resolve(writable + 56, 16), "partial copy overlap rejected"))
        return 1;
    allocator.reused = writable;
    for (int mode = 1; mode <= 6; ++mode) {
        allocator.mode = mode;
        if (!require(!port_gx_static_register(source_b, 64), "invalid allocator rejected") ||
            !require(!port_gx_static_resolve(source_b, 64), "failed allocation never published") ||
            !require(std::memcmp(staged_a, source_a + 32, 64) == 0, "failed allocation does not overwrite a live copy"))
            return 1;
    }
    allocator.mode = 0;
    const void* staged_b = port_gx_static_register(source_b, 64);
    if (!require(staged_b && staged_b != staged_a, "distinct source and failure rollback") ||
        !require(std::memcmp(staged_b, source_b, 64) == 0, "second source preserved"))
        return 1;
    unsigned char* mutable_mem1 = allocator.mem1 + 0x10000;
    mutable_mem1[0] = 11;
    const unsigned before_mutable = allocator.calls;
    if (!require(port_gx_static_register(mutable_mem1, 64) == mutable_mem1, "mutable MEM1 passes through") ||
        !require(port_gx_static_original(mutable_mem1, 64) == mutable_mem1, "mutable identity preserved"))
        return 1;
    mutable_mem1[0] = 23;
    if (!require(port_gx_static_resolve(mutable_mem1, 64) == mutable_mem1 && mutable_mem1[0] == 23 &&
                 allocator.calls == before_mutable, "mutable updates visible without staging") ||
        !require(!port_gx_static_resolve(allocator.mem1, 32), "physical null block rejected") ||
        !require(!port_gx_static_resolve(allocator.mem1 + window_bytes - 16, 32), "MEM1 boundary crossing rejected"))
        return 1;
    const unsigned char* staged_sphere = static_cast<const unsigned char*>(port_gx_static_register(sphere, sizeof(sphere)));
    if (!require(staged_sphere && std::memcmp(staged_sphere, sphere, sizeof(sphere)) == 0, "whole sphere blob and offsets") ||
        !require(port_gx_static_resolve(sphere + 0x760, sizeof(sphere) - 0x760) == staged_sphere + 0x760,
                 "positions keep their internal offset"))
        return 1;
    std::atomic<bool> threads_ok(true);
    std::vector<std::thread> threads;
    for (int n = 0; n < 4; ++n) {
        threads.push_back(std::thread([&threads_ok, staged_a]() {
            for (int i = 0; i < 1000; ++i)
                if (port_gx_static_resolve(source_a + 32, 64) != staged_a ||
                    port_gx_static_original(staged_a, 64) != source_a + 32)
                    threads_ok.store(false);
        }));
    }
    for (size_t i = 0; i < threads.size(); ++i) threads[i].join();
    if (!require(threads_ok.load(), "concurrent read-only resolve/reverse"))
        return 1;
    std::printf("PASS GX static memory: source=0x%llx staged=%p bytes=64 allocations=%u; MEM1/FIFO, immutable, offsets, failures, reentry, threads\n",
                static_cast<unsigned long long>(source_address), staged_a, allocator.calls);
    // Keep the window alive: the process-lifetime registry retains its ranges.
    return 0;
}

#ifndef SMS_GX_STATIC_MEMORY_TEST_NO_MAIN
int main() { return port_gx_static_memory_self_test(); }
#endif
