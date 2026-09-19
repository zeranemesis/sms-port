#include "port/recomp_probe.h"

#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME

#include "port/recomp_interrupt.h"

#include "aurora/lib/logging.hpp"

#include <array>
#include <chrono>
#include <string>

namespace sms::recomp::probe {
namespace {

aurora::Module Log("sms::recomp::probe");

// --- addresses ---------------------------------------------------------------
//
// Data symbols come from config/GMSP01/symbols.txt, cited by line number.
// Struct field offsets come from the decompiled headers, cited by file.

// gpApplication = .bss:0x803E10C0, size 0x4C (symbols.txt:24918).
// Field offsets from include/System/Application.hpp:74-76.
constexpr u32 kApplication = 0x803E10C0u;
constexpr u32 kApplicationDirector = kApplication + 0x04u;
constexpr u32 kApplicationAppState = kApplication + 0x08u; // u8

// symbols.txt:30627 and :30628 - the BOOT gate's own bookkeeping.
constexpr u32 kGameInit = 0x80405858u;
constexpr u32 kArcBufNLogo = 0x8040585Cu;

// gSetupThread = .bss:0x803F4388, size 0x310 (symbols.txt:28551). This is the
// thread TApplication::initialize() starts to run setupThreadFuncBoot(), and
// gameLoop() stays in APP_STATE_WAIT until OSIsThreadTerminated() says it has
// finished (src/System/Application.cpp:618).
constexpr u32 kSetupThread = 0x803F4388u;

// DVD layer, all .sbss (symbols.txt:31019, :31025, :31026, :31018).
constexpr u32 kDvdExecuting = 0x80405F50u;
constexpr u32 kDvdFatalErrorFlag = 0x80405F68u;
constexpr u32 kDvdCurrCommand = 0x80405F6Cu;
constexpr u32 kDvdThreadQueue = 0x80405F48u; // size 0x8

// Scheduler state (symbols.txt:30995, :30997) and the alarm queue (:30972).
constexpr u32 kRunQueueBits = 0x80405ED8u;
constexpr u32 kReschedule = 0x80405EE0u;
constexpr u32 kAlarmQueue = 0x80405E58u; // {head, tail}

// Low-memory OS globals, declared AT_ADDRESS in the decomp rather than living
// in symbols.txt: include/dolphin/os.h:63-64 and src/dolphin/os/OSContext.c:8.
constexpr u32 kActiveThreadQueue = 0x800000DCu; // {head, tail}
constexpr u32 kCurrentThread = 0x800000E4u;

// THPPlayer globals and worker threads, all verified in
// config/GMSP01/symbols.txt. These observations distinguish an empty decoded
// image from a renderer defect without changing THPPlayer itself.
constexpr u32 kActivePlayer = 0x803E3B20u;
constexpr u32 kReadThread = 0x803E4FF8u;
constexpr u32 kVideoDecodeThread = 0x803E6308u;
constexpr u32 kDecodedTextureSetQueue = 0x803E7638u;
constexpr u32 kActivePlayerOpen = kActivePlayer + 0xA0u;
constexpr u32 kActivePlayerState = kActivePlayer + 0xA4u;
constexpr u32 kActivePlayerInternalState = kActivePlayer + 0xA5u;
constexpr u32 kActivePlayerAudioExist = kActivePlayer + 0xA7u;
constexpr u32 kActivePlayerDvdError = kActivePlayer + 0xA8u;
constexpr u32 kActivePlayerVideoError = kActivePlayer + 0xACu;
constexpr u32 kActivePlayerVideoDecodeCount = kActivePlayer + 0xD8u;
// THPPlayer.h has curAudioTrack immediately before these fields.  The offsets
// are also independently visible in THPPlayerPrepare's recompiled accesses:
// it writes initOffset/initReadSize/initReadFrame at 0xB8/0xBC/0xC0, leaving
// curField at 0xC4 and the later counters at the offsets below.
constexpr u32 kActivePlayerCurrentVideo = kActivePlayer + 0xF0u;
constexpr u32 kActivePlayerCurrentAudio = kActivePlayer + 0xF4u;
constexpr u32 kActivePlayerDisplayTexture = kActivePlayer + 0xF8u;
constexpr u32 kMessageQueueUsedCount = 0x1Cu;

// OSThread field offsets - include/dolphin/os/OSThread.h:36-52. The embedded
// OSContext's own offsets are include/dolphin/os/OSContext.h's OS_CONTEXT_*.
constexpr u32 kThreadContextSp = 0x004u;   // context.gpr[1]
constexpr u32 kThreadContextLr = 0x084u;   // context.lr
constexpr u32 kThreadContextSrr0 = 0x198u; // context.srr0
constexpr u32 kThreadState = 0x2C8u;       // u16
constexpr u32 kThreadSuspend = 0x2CCu;     // s32
constexpr u32 kThreadPriority = 0x2D0u;    // s32
constexpr u32 kThreadQueue = 0x2DCu;       // OSThreadQueue*
constexpr u32 kThreadLinkActiveNext = 0x2FCu;
constexpr u32 kThreadSize = 0x310u;

// DVDCommandBlock field offsets - include/dolphin/dvd.h:19-32.
constexpr u32 kBlockCommand = 0x08u;
constexpr u32 kBlockState = 0x0Cu;
constexpr u32 kBlockOffset = 0x10u;
constexpr u32 kBlockLength = 0x14u;
constexpr u32 kBlockAddr = 0x18u;
constexpr u32 kBlockCurrTransferSize = 0x1Cu;
constexpr u32 kBlockTransferredSize = 0x20u;

// --- name tables -------------------------------------------------------------
//
// Only the threads and queues whose addresses are verified in symbols.txt.
// Anything else prints as a bare address, which is still resolvable offline -
// far better than an invented name.
struct Named {
    u32 address;
    const char *name;
};

constexpr Named kThreadNames[] = {
    { 0x803FA248u, "DefaultThread" }, // symbols.txt:28752
    { 0x803F9F38u, "IdleThread" },    // symbols.txt:28751
    { kSetupThread, "gSetupThread" }, // symbols.txt:28551
    { kReadThread, "THP ReadThread" }, // symbols.txt:25173
    { kVideoDecodeThread, "THP VideoDecodeThread" }, // symbols.txt:25176
};

constexpr Named kQueueNames[] = {
    { kDvdThreadQueue, "__DVDThreadQueue" }, // symbols.txt:31018
    { 0x80405FB0u, "retraceQueue" },         // symbols.txt:31042
};

const char *name_for(const Named *table, size_t count, u32 address)
{
    for (size_t i = 0; i < count; ++i) {
        if (table[i].address == address) {
            return table[i].name;
        }
    }
    return nullptr;
}

std::string describe(const Named *table, size_t count, u32 address)
{
    if (address == 0) {
        return "none";
    }
    if (const char *name = name_for(table, count, address)) {
        return fmt::format("{} ({:#010x})", name, address);
    }
    return fmt::format("{:#010x}", address);
}

// OS_THREAD_STATE, include/dolphin/os/OSThread.h:54-59.
const char *thread_state_name(u16 state)
{
    switch (state) {
        case 1: return "READY";
        case 2: return "RUNNING";
        case 4: return "WAITING";
        case 8: return "MORIBUND";
        default: return "?";
    }
}

// DVD_STATE_*, include/dolphin/dvd.h:155-167. Naming these matters: the whole
// point of the DVD line is to say whether the drive is healthy or has been
// declared dead, and a bare integer makes that a lookup every time.
const char *dvd_state_name(s32 state)
{
    switch (state) {
        case -1: return "FATAL_ERROR";
        case 0: return "END";
        case 1: return "BUSY";
        case 2: return "WAITING";
        case 3: return "COVER_CLOSED";
        case 4: return "NO_DISK";
        case 5: return "COVER_OPEN";
        case 6: return "WRONG_DISK";
        case 7: return "MOTOR_STOPPED";
        case 8: return "PAUSING";
        case 9: return "IGNORED";
        case 10: return "CANCELED";
        case 11: return "RETRY";
        default: return "?";
    }
}

// TApplication's own enum, include/System/Application.hpp:61-71.
const char *app_state_name(u8 state)
{
    switch (state) {
        case 0: return "WAIT";
        case 1: return "DEFAULT";
        case 2: return "BOOT";
        case 3: return "NLOGO";
        case 4: return "DONE";
        case 5: return "GAMEPLAY";
        case 6: return "MOVIE";
        case 7: return "QUIT";
        case 8: return "TITLE";
        case 9: return "MENU";
        default: return "?";
    }
}

// A pointer is only followed once it is inside guest RAM, 4-byte aligned, and
// leaves room for the whole struct. A corrupted list must produce a short,
// honest report rather than a crash or a wall of garbage.
bool readable(const CPUState *cpu, u32 address, u32 size)
{
    if (address < GC_RAM_BASE || (address & 3u) != 0) {
        return false;
    }
    const u64 offset = static_cast<u64>(address) - GC_RAM_BASE;
    return offset + size <= cpu->ram_size;
}

void report_gates(CPUState *cpu)
{
    const u8 appState = mem_read8(cpu, kApplicationAppState);
    const u32 director = mem_read32(cpu, kApplicationDirector);
    const u32 gameInit = mem_read32(cpu, kGameInit);
    const u32 arcBufNLogo = mem_read32(cpu, kArcBufNLogo);

    const u16 setupState = mem_read16(cpu, kSetupThread + kThreadState);
    const u32 setupQueue = mem_read32(cpu, kSetupThread + kThreadQueue);
    const u32 setupSrr0 = mem_read32(cpu, kSetupThread + kThreadContextSrr0);

    Log.info("gates: appState={} ({}) director={:#010x} sGameInit={:#x} arcBufNLogo={:#010x} | "
             "gSetupThread state={} ({}) queue={} srr0={:#010x}",
        appState, app_state_name(appState), director, gameInit, arcBufNLogo, setupState,
        thread_state_name(setupState), describe(kQueueNames, std::size(kQueueNames), setupQueue), setupSrr0);
}

void report_dvd(CPUState *cpu)
{
    const u32 executing = mem_read32(cpu, kDvdExecuting);
    const s32 fatal = static_cast<s32>(mem_read32(cpu, kDvdFatalErrorFlag));
    const u32 currCommand = mem_read32(cpu, kDvdCurrCommand);

    std::string block = "none";
    if (readable(cpu, executing, 0x30u)) {
        const s32 state = static_cast<s32>(mem_read32(cpu, executing + kBlockState));
        block = fmt::format("cmd={} state={} ({}) offset={:#x} length={:#x} addr={:#010x} curr={:#x} done={:#x}",
            mem_read32(cpu, executing + kBlockCommand), state, dvd_state_name(state),
            mem_read32(cpu, executing + kBlockOffset), mem_read32(cpu, executing + kBlockLength),
            mem_read32(cpu, executing + kBlockAddr), mem_read32(cpu, executing + kBlockCurrTransferSize),
            mem_read32(cpu, executing + kBlockTransferredSize));
    }

    Log.info("dvd: executing={:#010x} fatalError={} currCommand={} | {} | piCause={:#010x} piMask={:#010x}", executing,
        fatal, currCommand, block, interrupt::debug_cause(), interrupt::debug_mask());
}

void report_threads(CPUState *cpu)
{
    const u32 current = mem_read32(cpu, kCurrentThread);
    Log.info("threads: current={} runQueueBits={:#010x} reschedule={} alarmQueueHead={:#010x}",
        describe(kThreadNames, std::size(kThreadNames), current), mem_read32(cpu, kRunQueueBits),
        static_cast<s32>(mem_read32(cpu, kReschedule)), mem_read32(cpu, kAlarmQueue));

    // Bounded three ways: a hard iteration cap, a validity check before every
    // dereference, and a visited set so a cyclic list terminates instead of
    // printing until the disk fills.
    constexpr size_t kMaxThreads = 32;
    std::array<u32, kMaxThreads> visited {};
    size_t visitedCount = 0;

    u32 thread = mem_read32(cpu, kActiveThreadQueue);
    while (thread != 0 && visitedCount < kMaxThreads) {
        if (!readable(cpu, thread, kThreadSize)) {
            Log.info("threads:   {:#010x} is not a usable thread pointer, stopping", thread);
            return;
        }
        bool seen = false;
        for (size_t i = 0; i < visitedCount; ++i) {
            if (visited[i] == thread) {
                seen = true;
                break;
            }
        }
        if (seen) {
            Log.info("threads:   list loops back to {:#010x}, stopping", thread);
            return;
        }
        visited[visitedCount++] = thread;

        const u16 state = mem_read16(cpu, thread + kThreadState);
        Log.info("threads:   {} state={} ({}) prio={} suspend={} queue={} srr0={:#010x} lr={:#010x} sp={:#010x}",
            describe(kThreadNames, std::size(kThreadNames), thread), state, thread_state_name(state),
            static_cast<s32>(mem_read32(cpu, thread + kThreadPriority)),
            static_cast<s32>(mem_read32(cpu, thread + kThreadSuspend)),
            describe(kQueueNames, std::size(kQueueNames), mem_read32(cpu, thread + kThreadQueue)),
            mem_read32(cpu, thread + kThreadContextSrr0), mem_read32(cpu, thread + kThreadContextLr),
            mem_read32(cpu, thread + kThreadContextSp));

        thread = mem_read32(cpu, thread + kThreadLinkActiveNext);
    }
}

void report_thp(CPUState *cpu)
{
    const u32 display = mem_read32(cpu, kActivePlayerDisplayTexture);
    Log.info("thp: open={} state={} internal={} audio={} dvdError={} videoError={} decodeCount={} v/a={}/{} display={:#010x} "
             "decodedQueue={} | read={} pc={:#010x} video={} pc={:#010x}",
        mem_read32(cpu, kActivePlayerOpen), mem_read8(cpu, kActivePlayerState),
        mem_read8(cpu, kActivePlayerInternalState), mem_read8(cpu, kActivePlayerAudioExist),
        static_cast<s32>(mem_read32(cpu, kActivePlayerDvdError)),
        static_cast<s32>(mem_read32(cpu, kActivePlayerVideoError)),
        static_cast<s32>(mem_read32(cpu, kActivePlayerVideoDecodeCount)),
        static_cast<s32>(mem_read32(cpu, kActivePlayerCurrentVideo)),
        static_cast<s32>(mem_read32(cpu, kActivePlayerCurrentAudio)),
        display,
        static_cast<s32>(mem_read32(cpu, kDecodedTextureSetQueue + kMessageQueueUsedCount)),
        thread_state_name(mem_read16(cpu, kReadThread + kThreadState)),
        mem_read32(cpu, kReadThread + kThreadContextSrr0),
        thread_state_name(mem_read16(cpu, kVideoDecodeThread + kThreadState)),
        mem_read32(cpu, kVideoDecodeThread + kThreadContextSrr0));
}

} // namespace

void report(CPUState *cpu)
{
    // Wall-clock throttled and hard-capped. Deliberately not gated on a frame
    // count: frame rate is one of the things under investigation, so a
    // frame-based gate would change cadence exactly when the run got
    // interesting.
    static auto lastReport = std::chrono::steady_clock::now();
    static unsigned reports = 0;
    constexpr unsigned kMaxReports = 120;

    if (reports >= kMaxReports) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (reports != 0 && now - lastReport < std::chrono::seconds(1)) {
        return;
    }
    lastReport = now;
    ++reports;

    report_gates(cpu);
    report_dvd(cpu);
    report_threads(cpu);
    report_thp(cpu);
}

} // namespace sms::recomp::probe

#else // !DOLPHINJET_HAVE_RECOMPILED_GAME

namespace sms::recomp::probe {
void report(CPUState *) { }
} // namespace sms::recomp::probe

#endif
