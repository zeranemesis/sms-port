// Standalone Android shell diagnostic. No game assets or saves are accessed.
// This validates prerequisites, not the complete game's pointer model.
#include <cerrno>
#include <cinttypes>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string>
#include <cstdarg>
#ifdef SMS_PROBE_JNI
#include <jni.h>
#endif

#if !defined(__ANDROID__) || (!defined(__aarch64__) && !defined(__x86_64__))
#error Android ARM64 or x86_64 required
#endif

// Same non-destructive address-placement policy as port_runtime.cpp.
static void* mapExact(uintptr_t address, size_t size) {
    void* want = reinterpret_cast<void*>(address);
#ifdef MAP_FIXED_NOREPLACE
    constexpr int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE;
#else
    constexpr int flags = MAP_PRIVATE | MAP_ANONYMOUS;
#endif
    void* result = mmap(want, size, PROT_READ | PROT_WRITE, flags, -1, 0);
    if (result == want) return result;
    if (result != MAP_FAILED) munmap(result, size);
    return MAP_FAILED;
}

struct StackProbe { uintptr_t start, end, local; };
int port_spc_registry_self_test(bool require_high_pointer);
static void* threadMain(void* argument) {
    auto* probe = static_cast<StackProbe*>(argument);
    volatile uint32_t local = 0x12345678;
    probe->local = reinterpret_cast<uintptr_t>(&local);
    return nullptr;
}

static void append(std::string& report, const char* format, ...) {
    char buffer[1024];
    va_list arguments;
    va_start(arguments, format);
    std::vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    report += buffer;
}

static int runMemoryProbe(std::string& report) {
    const long page = sysconf(_SC_PAGESIZE);
#if defined(__aarch64__)
    constexpr const char* architecture = "ARM64";
#else
    constexpr const char* architecture = "x86_64 (emulator prerequisite test, not ARM64 validation)";
#endif
    append(report, "Android %s pointer_bits=%zu page_size=%ld\n", architecture, sizeof(void*) * 8, page);
    if (page <= 0) return 1;
    constexpr size_t memSize = 64u << 20;
    void* mem = mapExact(0x80000000u, memSize);
    if (mem == MAP_FAILED) {
        append(report, "FAIL MEM1 placement errno=%d (%s)\n", errno, std::strerror(errno));
        return 1;
    }
    auto* bytes = static_cast<volatile uint8_t*>(mem);
    for (size_t offset = 0; offset < memSize; offset += static_cast<size_t>(page))
        bytes[offset] = static_cast<uint8_t>((offset / page) ^ 0x5a);
    bool valid = true;
    for (size_t offset = 0; offset < memSize; offset += static_cast<size_t>(page))
        valid &= bytes[offset] == static_cast<uint8_t>((offset / page) ^ 0x5a);
    append(report, "%s MEM1 address=%p size=%zu page_write_read\n", valid ? "PASS" : "FAIL", mem, memSize);

    constexpr size_t stackSize = 1u << 20;
    void* stack = MAP_FAILED;
    for (uintptr_t address = 0x40000000u; address + stackSize <= 0x80000000u; address += stackSize) {
        stack = mapExact(address, stackSize);
        if (stack != MAP_FAILED) break;
    }
    if (stack == MAP_FAILED) {
        append(report, "FAIL no low thread stack available\n");
        munmap(mem, memSize);
        return 1;
    }
    StackProbe probe{reinterpret_cast<uintptr_t>(stack), reinterpret_cast<uintptr_t>(stack) + stackSize, 0};
    pthread_attr_t attributes;
    int rc = pthread_attr_init(&attributes);
    if (rc) {
        append(report, "FAIL pthread_attr_init=%d\n", rc);
        munmap(stack, stackSize); munmap(mem, memSize); return 1;
    }
    rc = pthread_attr_setstack(&attributes, stack, stackSize);
    pthread_t thread{};
    if (!rc) rc = pthread_create(&thread, &attributes, threadMain, &probe);
    pthread_attr_destroy(&attributes);
    if (!rc) {
        const int joined = pthread_join(thread, nullptr);
        // Do not unmap a potentially live thread's stack if joining failed.
        if (joined) { append(report, "FAIL pthread_join=%d\n", joined); return 1; }
    }
    const bool stackOK = !rc && probe.local >= probe.start && probe.local < probe.end && probe.local < 0x80000000u;
    append(report, "%s low_stack rc=%d local=0x%" PRIxPTR "\n", stackOK ? "PASS" : "FAIL", rc, probe.local);
    static int global;
    append(report, "INFO global=%p code=0x%" PRIxPTR " (full game relocation audit remains required)\n",
                static_cast<void*>(&global), reinterpret_cast<uintptr_t>(&runMemoryProbe));
    munmap(stack, stackSize);
    munmap(mem, memSize);
    const int spc = port_spc_registry_self_test(true);
    append(report, "%s SPC registry high-pointer, typed callbacks, tokens and reentrancy\n",
           spc == 0 ? "PASS" : "FAIL");
    return valid && stackOK && spc == 0 ? 0 : 1;
}

#ifdef SMS_PROBE_JNI
extern "C" JNIEXPORT jstring JNICALL
Java_org_smsunified_diagnostic_ProbeActivity_nativeRun(JNIEnv* env, jclass) {
    std::string report;
    const int result = runMemoryProbe(report);
    report = std::string(result == 0 ? "RESULT PASS\n" : "RESULT FAIL\n") + report;
    report += "Context: Android application JNI. Full game and GPU not validated.\n";
    return env->NewStringUTF(report.c_str());
}
#else
int main() {
    std::string report;
    const int result = runMemoryProbe(report);
    std::fputs(report.c_str(), stdout);
    std::puts("NOTE shell process only: repeat these checks inside the eventual Android application.");
    return result;
}
#endif
