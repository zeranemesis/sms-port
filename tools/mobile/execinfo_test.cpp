// Android <33 shim test. No game, assets, saves or graphics.
#include <dlfcn.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>

static ssize_t probe_write(int fd, const void* data, size_t length);
static int probe_dladdr(const void* address, Dl_info* info);
#define write probe_write
#define dladdr probe_dladdr
#include "../../src/port_include/execinfo.h"
#undef dladdr
#undef write

#if !defined(__ANDROID__) || __ANDROID_API__ >= 33
#error This test exercises the Android API <33 implementation.
#endif

namespace {
const int capture_fd = -777;
std::string captured;
std::string long_module(900, 'm');
int write_mode;
int write_calls;
bool fake_module;
bool require(bool value, const char* message)
{
    if (!value)
        std::fprintf(stderr, "FAIL execinfo: %s\n", message);
    return value;
}
}

static ssize_t probe_write(int fd, const void* data, size_t length)
{
    if (fd != capture_fd)
        return write(fd, data, length);
    ++write_calls;
    if (write_mode == 2)
        return 0;
    if (write_mode == 3) {
        errno = EPIPE;
        return -1;
    }
    if (write_calls == 1) {
        errno = EINTR;
        return -1;
    }
    size_t written = std::min(length, size_t(3));
    captured.append(static_cast<const char*>(data), written);
    return static_cast<ssize_t>(written);
}

static int probe_dladdr(const void* address, Dl_info* info)
{
    if (fake_module) {
        std::memset(info, 0, sizeof(*info));
        info->dli_fname = long_module.c_str();
        info->dli_sname = "probe_symbol";
        info->dli_fbase = const_cast<void*>(address);
        info->dli_saddr = const_cast<void*>(address);
        return 1;
    }
    return dladdr(address, info);
}

extern "C" __attribute__((noinline)) int execinfo_test_anchor(void** frames, int capacity)
{
    int n = backtrace(frames, capacity);
    // Keep the call in the anchor rather than allowing a tail-call to backtrace.
    volatile int captured_count = n;
    return captured_count;
}

static bool check_symbols(void* const* frames, int count)
{
    char** symbols = backtrace_symbols(frames, count);
    if (!require(symbols != NULL, "symbols allocated"))
        return false;
    std::string expected;
    const char* prior = reinterpret_cast<const char*>(symbols + count);
    for (int i = 0; i < count; ++i) {
        if (!require(symbols[i] == prior, "single contiguous string allocation") ||
            !require(std::strstr(symbols[i], " [") != NULL, "address formatted")) {
            std::free(symbols);
            return false;
        }
        expected += symbols[i];
        expected += '\n';
        prior = symbols[i] + std::strlen(symbols[i]) + 1;
    }
    std::free(symbols);
    captured.clear(); write_calls = 0; write_mode = 1;
    backtrace_symbols_fd(frames, count, capture_fd);
    return require(captured == expected, "fd output complete across EINTR and short writes") &&
           require(write_calls > count, "write retry path exercised");
}

int main()
{
    void* frames[32] = {};
    int count = execinfo_test_anchor(frames, 32);
    if (!require(count >= 2 && count <= 32, "real unwind produces multiple bounded frames") ||
        !require(frames[0] != NULL, "first frame nonnull") ||
        !require(backtrace(frames, 0) == 0, "zero capacity") ||
        !require(backtrace(NULL, 32) == 0, "null buffer") ||
        !require(execinfo_test_anchor(frames, 1) == 1, "one-frame capacity"))
        return 1;
    count = execinfo_test_anchor(frames, 32);
    Dl_info first;
    if (!require(dladdr(frames[0], &first) != 0 && first.dli_sname &&
                 std::strcmp(first.dli_sname, "execinfo_test_anchor") == 0,
                 "shim excluded: first captured frame is calling anchor"))
        return 1;
    if (!check_symbols(frames, count))
        return 1;
    fake_module = true;
    if (!check_symbols(frames, count))
        return 1;
    fake_module = false;
    void* unknown[1] = { reinterpret_cast<void*>(uintptr_t(0x1234)) };
    if (!check_symbols(unknown, 1) ||
        !require(backtrace_symbols(frames, 0) == NULL, "zero symbol count") ||
        !require(backtrace_symbols(NULL, 1) == NULL, "null symbol buffer"))
        return 1;
    captured.clear(); write_calls = 0; write_mode = 2; errno = 0;
    backtrace_symbols_fd(frames, count, capture_fd);
    if (!require(write_calls == 1 && captured.empty() && errno == EIO, "zero write stops with EIO"))
        return 1;
    write_calls = 0; write_mode = 3; errno = 0;
    backtrace_symbols_fd(frames, count, capture_fd);
    if (!require(write_calls == 1 && errno == EPIPE, "hard write error stops and survives"))
        return 1;
    std::printf("PASS Android execinfo API%d: frames=%d, anchor, symbols/free, long paths, EINTR/partial/zero/error writes\n",
                __ANDROID_API__, count);
    return 0;
}
