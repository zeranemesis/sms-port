// SMS_CPU_SAMPLE=<file>: a sampling CPU profiler for diagnosis, off unless the
// variable is set. A thread suspends every other thread of the process about
// once a millisecond, walks its stack with the x64 unwind tables and writes
//
//   s <thread id> <display copy> <pc> <return address> ...
//
// one line per sample. At exit it appends the loaded modules
//
//   m <base> <size> <path>
//
// so tools/cpu-profile.py can name the addresses (nm for the MinGW game,
// dbghelp for the MSVC DLLs). Needs no administrator rights, unlike ETW.
#ifdef _WIN64
// K32EnumProcessModules lives in kernel32: no psapi import library needed.
#define PSAPI_VERSION 2
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

extern "C" unsigned int GXPC_FrameCount(void) __attribute__((weak));

namespace {

std::atomic<bool> g_run{false};
std::thread g_thread;
FILE* g_out = nullptr;

constexpr int kMaxDepth = 48;

int walk(CONTEXT& ctx, DWORD64* pcs)
{
    int depth = 0;
    while (depth < kMaxDepth && ctx.Rip != 0) {
        pcs[depth++] = ctx.Rip;
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
        if (!fn) {
            // Only the interrupted function can be a leaf (its return address
            // at the top of the stack); deeper, no unwind data means the walk
            // has left known code, so stop rather than read a stray pointer.
            if (depth > 1)
                break;
            ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp);
            ctx.Rsp += 8;
            continue;
        }
        PVOID handlerData = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fn, &ctx, &handlerData,
                         &establisher, nullptr);
    }
    return depth;
}

void sample_loop()
{
    const DWORD self = GetCurrentThreadId();
    const DWORD pid = GetCurrentProcessId();
    std::vector<DWORD> threads;
    ULONGLONG lastScan = 0;
    DWORD64 pcs[kMaxDepth];
    // A high-resolution timer: Sleep(1) waits a whole 15.6 ms tick by default.
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                          TIMER_ALL_ACCESS);
    while (g_run.load(std::memory_order_relaxed)) {
        const ULONGLONG now = GetTickCount64();
        if (now - lastScan > 500) {
            lastScan = now;
            threads.clear();
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            THREADENTRY32 te{sizeof te};
            if (snap != INVALID_HANDLE_VALUE && Thread32First(snap, &te)) {
                do {
                    if (te.th32OwnerProcessID == pid && te.th32ThreadID != self)
                        threads.push_back(te.th32ThreadID);
                } while (Thread32Next(snap, &te));
            }
            if (snap != INVALID_HANDLE_VALUE)
                CloseHandle(snap);
        }
        const unsigned frame = GXPC_FrameCount ? GXPC_FrameCount() : 0;
        for (DWORD tid : threads) {
            HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION,
                                  FALSE, tid);
            if (!h)
                continue;
            if (SuspendThread(h) != (DWORD)-1) {
                CONTEXT ctx{};
                ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                int depth = 0;
                if (GetThreadContext(h, &ctx))
                    depth = walk(ctx, pcs);
                ResumeThread(h);
                if (depth > 0) {
                    std::fprintf(g_out, "s %lu %u", (unsigned long)tid, frame);
                    for (int i = 0; i < depth; ++i)
                        std::fprintf(g_out, " %llx", (unsigned long long)pcs[i]);
                    std::fputc('\n', g_out);
                }
            }
            CloseHandle(h);
        }
        LARGE_INTEGER due;
        due.QuadPart = -10000; // 1 ms, relative
        if (timer && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
            WaitForSingleObject(timer, INFINITE);
        else
            Sleep(1);
    }
    if (timer)
        CloseHandle(timer);
}

void write_modules()
{
    HMODULE mods[512];
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), mods, sizeof mods, &needed))
        return;
    for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 512; ++i) {
        MODULEINFO info{};
        char path[MAX_PATH];
        if (GetModuleInformation(GetCurrentProcess(), mods[i], &info, sizeof info) &&
            GetModuleFileNameA(mods[i], path, MAX_PATH))
            std::fprintf(g_out, "m %llx %lx %s\n", (unsigned long long)(uintptr_t)info.lpBaseOfDll,
                         (unsigned long)info.SizeOfImage, path);
    }
}

void stop()
{
    if (!g_run.exchange(false))
        return;
    if (g_thread.joinable())
        g_thread.join();
    write_modules();
    std::fclose(g_out);
    g_out = nullptr;
}

struct Starter {
    Starter()
    {
        const char* path = std::getenv("SMS_CPU_SAMPLE");
        if (!path || !*path || !(g_out = std::fopen(path, "w")))
            return;
        std::setvbuf(g_out, nullptr, _IOFBF, 1 << 20);
        g_run = true;
        g_thread = std::thread(sample_loop);
        std::atexit(stop);
    }
} g_starter;

} // namespace
#endif
