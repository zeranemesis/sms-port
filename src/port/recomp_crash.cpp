#include "port/recomp_crash.h"

#include "aurora/lib/logging.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>

#ifdef _WIN32
#include <windows.h>
// After windows.h, which dbghelp.h requires.
#include <dbghelp.h>
#else
#include <csignal>
#if defined(__GLIBC__) || defined(__APPLE__)
#include <execinfo.h>
#endif
#endif

namespace sms::recomp::crash {
namespace {

aurora::Module Log("sms::recomp::crash");

// A hard crash in this port leaves no trace at all: no FATAL line, no exit
// code, nothing in stderr - the process simply stops. That has already cost
// two investigations that could only guess at a location, and no command-line
// debugger is installed on this machine (cdb, windbg and ntsd are all absent),
// so the only way to learn where a fault happens is to catch it in-process.
//
// This is diagnostics, not recovery: it reports and then lets the fault take
// its normal course. Nothing here tries to continue after an access violation.

constexpr unsigned kMaxFrames = 48;

void flush_logs()
{
    // A fatal path that flushes only stderr throws away everything the run
    // wrote to stdout, which is where every probe in this port logs. That
    // mistake has been made here before.
    std::fflush(stdout);
    std::fflush(stderr);
}

#ifdef _WIN32

const char *exception_name(DWORD code)
{
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "access violation";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "datatype misalignment";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
        case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
        case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
        case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
        case 0xC0000409u: return "stack buffer overrun / __fastfail";
        case 0xC0000374u: return "heap corruption";
        default: return "unknown";
    }
}

bool is_fatal(DWORD code)
{
    // A vectored handler sees every exception, including the ones C++ and the
    // debugger machinery raise in normal operation. Reporting those would bury
    // the one that matters.
    //
    // Listing only the well-known codes was too narrow and reported nothing at
    // all on a real crash, so the rule is now the severity field: anything
    // whose top two bits are set is an error status. Two codes are excluded
    // explicitly - a C++ throw and a thread-name notification both use that
    // severity in normal operation.
    constexpr DWORD kCppException = 0xE06D7363u;
    constexpr DWORD kThreadNameException = 0x406D1388u;
    if (code == kCppException || code == kThreadNameException) {
        return false;
    }
    return (code & 0xC0000000u) == 0xC0000000u;
}

void report_stack()
{
    const HANDLE process = GetCurrentProcess();
    // Symbols are only as good as the build: a Release build without /Zi has
    // no PDB, and the report then gives module plus offset, which still names
    // the culprit once mapped. Say which of the two this is rather than
    // silently printing bare addresses.
    const bool symbols = SymInitialize(process, nullptr, TRUE) != FALSE;
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);

    void *frames[kMaxFrames] = {};
    const USHORT captured = CaptureStackBackTrace(0, kMaxFrames, frames, nullptr);

    alignas(SYMBOL_INFO) char symbolStorage[sizeof(SYMBOL_INFO) + 512] = {};
    auto *symbol = reinterpret_cast<SYMBOL_INFO *>(symbolStorage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 511;

    for (USHORT i = 0; i < captured; ++i) {
        const auto address = reinterpret_cast<DWORD64>(frames[i]);
        DWORD64 displacement = 0;
        IMAGEHLP_LINE64 line {};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;

        const bool named = symbols && SymFromAddr(process, address, &displacement, symbol) != FALSE;
        const bool located = symbols && SymGetLineFromAddr64(process, address, &lineDisplacement, &line) != FALSE;

        if (named && located) {
            Log.error("  #{:<2} {:#018x} {}+{:#x}  ({}:{})", i, address, symbol->Name, displacement, line.FileName,
                line.LineNumber);
        } else if (named) {
            Log.error("  #{:<2} {:#018x} {}+{:#x}", i, address, symbol->Name, displacement);
        } else {
            Log.error("  #{:<2} {:#018x}  <no symbol - build with debug info to name this>", i, address);
        }
    }

    if (symbols) {
        SymCleanup(process);
    }
}

void abort_handler(int)
{
    Log.error("host crash: abort() - an assertion or an unhandled C++ exception");
    report_stack();
    flush_logs();
    std::_Exit(134);
}

void terminate_handler()
{
    Log.error("host crash: std::terminate - an exception escaped");
    report_stack();
    flush_logs();
    std::_Exit(134);
}

LONG WINAPI unhandled_filter(EXCEPTION_POINTERS *info)
{
    if (info != nullptr && info->ExceptionRecord != nullptr) {
        Log.error("host crash: unhandled exception {:#010x} at {:#018x}",
            static_cast<unsigned long>(info->ExceptionRecord->ExceptionCode),
            reinterpret_cast<DWORD64>(info->ExceptionRecord->ExceptionAddress));
    }
    report_stack();
    flush_logs();
    return EXCEPTION_EXECUTE_HANDLER;
}

LONG WINAPI vectored_handler(EXCEPTION_POINTERS *info)
{
    if (info == nullptr || info->ExceptionRecord == nullptr) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const DWORD code = info->ExceptionRecord->ExceptionCode;
    if (!is_fatal(code)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // Report once. A fault inside the reporting path would otherwise recurse.
    static bool reported = false;
    if (reported) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    reported = true;

    Log.error("host crash: {} ({:#010x}) at {:#018x}", exception_name(code), static_cast<unsigned long>(code),
        reinterpret_cast<DWORD64>(info->ExceptionRecord->ExceptionAddress));
    if (code == EXCEPTION_ACCESS_VIOLATION && info->ExceptionRecord->NumberParameters >= 2) {
        const ULONG_PTR operation = info->ExceptionRecord->ExceptionInformation[0];
        const ULONG_PTR address = info->ExceptionRecord->ExceptionInformation[1];
        Log.error("  {} address {:#018x}", operation == 0 ? "reading" : (operation == 1 ? "writing" : "executing"),
            static_cast<DWORD64>(address));
    }
    report_stack();
    flush_logs();

    // Let it die the way it would have. This reports, it does not recover:
    // continuing after an access violation would turn a located fault into an
    // unlocated one later.
    return EXCEPTION_CONTINUE_SEARCH;
}

#else // !_WIN32

void posix_handler(int signal)
{
    static bool reported = false;
    if (reported) {
        std::_Exit(128 + signal);
    }
    reported = true;

    Log.error("host crash: signal {}", signal);
#if defined(__GLIBC__) || defined(__APPLE__)
    void *frames[kMaxFrames] = {};
    const int captured = backtrace(frames, kMaxFrames);
    flush_logs();
    // Writes straight to the descriptor, which is the only backtrace call that
    // is safe from a signal handler.
    backtrace_symbols_fd(frames, captured, 2);
#else
    Log.error("  no backtrace available on this platform");
#endif
    flush_logs();
    std::_Exit(128 + signal);
}

#endif

} // namespace

void install()
{
#ifdef _WIN32
    // First in the chain, so this reports before anything else swallows the
    // exception.
    AddVectoredExceptionHandler(1, &vectored_handler);
    // A vectored handler does not see abort(), which is how an ASSERT or an
    // unhandled C++ exception ends. Cover those too, or a whole class of
    // failure stays as silent as it was.
    SetUnhandledExceptionFilter(&unhandled_filter);
    std::signal(SIGABRT, &abort_handler);
    std::set_terminate(&terminate_handler);
#else
    std::signal(SIGSEGV, &posix_handler);
    std::signal(SIGBUS, &posix_handler);
    std::signal(SIGILL, &posix_handler);
    std::signal(SIGFPE, &posix_handler);
#endif
    // Straight to stderr, not through the logger: at this point in main() the
    // logging system is not up yet and the line is simply lost, which is how
    // the first version of this looked like it had never run at all.
    std::fputs("[info] [sms::recomp::crash] crash reporter installed\n", stderr);
    std::fflush(stderr);
}

} // namespace sms::recomp::crash
