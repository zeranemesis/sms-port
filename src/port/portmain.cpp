// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm
//
// This is the Aurora bootstrap for the SMS native PC port: it brings up a
// window, the Party-Board-style menu (F1), and an update/render loop. When
// generated/ exists (tools/port/recompile.py has run against a real GMSP01
// dump - see docs/recompilation.md and docs/port_bootstrap.md), it also
// opens the disc image configured in Settings -> Prelaunch -> Disc Image
// and drives the recompiled game through include/port/recomp_boot.h.
// Without a dump, DOLPHINJET_HAVE_RECOMPILED_GAME is undefined and this
// file builds exactly as before (menu shell only).
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "ui/menu_bar.hpp"
#include "ui/overlay.hpp"
#include "ui/ui.hpp"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/lib/logging.hpp>
#include <dolphin/gx/GXAurora.h>
#include <dolphin/vi.h>
#include <port/config.hpp>
#include <port/main.h>
#include <port/settings.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_timer.h>

#include <cstdio>
#include <cstdlib>

#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME
#include <aurora/dvd.h>
#include <dolphin/dvd.h>
#include <port/recomp_boot.h>
#endif

namespace sms {
bool IsRunning = true;
}

std::filesystem::path sms::ConfigPath;

aurora::Module SmsMainLog("sms::main");

static std::filesystem::path calculate_config_path()
{
    char *prefPath = SDL_GetPrefPath("dolphinjet", "DolphinJet");
    if (prefPath == nullptr) {
        SmsMainLog.error("Unable to get preferences path: {}", SDL_GetError());
        return {};
    }
    std::filesystem::path path = reinterpret_cast<const char8_t *>(prefPath);
    SDL_free(prefPath);
    return path;
}

static void aurora_log_callback(AuroraLogLevel level, const char *module, const char *message, unsigned int len)
{
    const char *levelStr = "??";
    FILE *out = stdout;
    switch (level) {
        case LOG_DEBUG: levelStr = "DEBUG"; break;
        case LOG_INFO: levelStr = "INFO"; break;
        case LOG_WARNING: levelStr = "WARNING"; break;
        case LOG_ERROR: out = stderr; levelStr = "ERROR"; break;
        case LOG_FATAL: out = stderr; levelStr = "FATAL"; break;
    }
    std::fprintf(out, "[%s] %s: %.*s\n", levelStr, module, static_cast<int>(len), message);
    if (level == LOG_FATAL) {
        // Flush BOTH streams, not just the one this message went to. INFO and
        // WARNING go to stdout, which is fully buffered when redirected to a
        // file, so aborting after flushing stderr alone threw away every
        // buffered line leading up to the failure - the exact history needed
        // to explain it. Measured: a fatal that followed ~1500 log lines left
        // a zero-byte stdout capture behind.
        std::fflush(stdout);
        std::fflush(stderr);
        std::abort();
    }
}

static AuroraBackend resolve_desired_backend()
{
    const auto &configured = sms::getSettings().backend.graphicsBackend.getValue();
    if (configured == "d3d12") return BACKEND_D3D12;
    if (configured == "metal") return BACKEND_METAL;
    if (configured == "vulkan") return BACKEND_VULKAN;
    if (configured == "opengl") return BACKEND_OPENGL;
    if (configured == "opengles") return BACKEND_OPENGLES;
    if (configured == "webgpu") return BACKEND_WEBGPU;
    if (configured == "null") return BACKEND_NULL;
    return BACKEND_AUTO;
}

#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME
namespace {
CPUState g_gameCpu {};
bool g_gameRunning = false;

// dolrecomp_run_blocks' "blocks" are calls into the top-level chassis
// dispatch (generated.h's dolrecomp_find_original), not PPC instructions -
// each one runs an entire recompiled chunk function until it hits a
// cross-chunk branch or an unresolved host call. There is no real game
// running yet to measure a per-frame budget against (see
// docs/port_bootstrap.md's verification section - this is unexecuted), so
// this is a conservative starting guess, not a tuned constant: small
// enough that a stuck/looping region can't wedge the host's own render
// loop for long, logged loudly if it turns out too small to make
// progress. Expect to revisit once this has actually been run once.
constexpr unsigned kGameBlocksPerFrame = 131072;

// Host performance-counter ticks spent inside step_game, against the
// wall-clock second the heartbeat covers.
Uint64 g_stepTicks = 0;

// Opens the configured disc image and boots the recompiled game. Called
// once, before the first frame - see port_main(). Logs and leaves
// g_gameRunning false on any failure, which keeps the menu-only loop as
// the fallback exactly as it was before this game code existed.
void try_boot_game()
{
    const auto discPath = sms::getSettings().backend.discPath.getValue();
    if (discPath.empty()) {
        SmsMainLog.info("No disc image configured (Settings -> Prelaunch) - menu only");
        return;
    }
    if (!aurora_dvd_open(discPath.c_str())) {
        SmsMainLog.error("Failed to open disc image '{}'", discPath);
        return;
    }
    DVDInit();
    g_gameRunning = sms::recomp::boot_game(&g_gameCpu);
    if (!g_gameRunning) {
        SmsMainLog.error("Failed to boot the recompiled game from '{}' - falling back to the menu-only loop", discPath);
    }
}
} // namespace
#endif

// Runs the Aurora event/render loop with the Party-Board-style menu on
// screen. When DOLPHINJET_HAVE_RECOMPILED_GAME is defined and a disc image
// booted successfully (try_boot_game(), called once from port_main()), the
// recompiled game is stepped once per frame underneath the menu/overlay -
// otherwise this is exactly the menu-only loop it always was.
static bool run_menu_loop()
{
#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME
    // Guest frame pacing, read before every present - see the block inside the
    // loop for why the decision has to precede aurora_begin_frame().
    //
    // The period comes from include/port/recomp_boot.h, where it sits next to
    // the __OSTVMode format byte, because the two have to agree: the host
    // paces the guest and the host is what raises its VI retrace interrupts, so
    // a 50Hz disc paced at 60Hz gets a sixth more retraces per second than a
    // real PAL console delivers, and everything the guest times against
    // OSGetTime runs fast. This was the constant half of the mismatch
    // docs/port_todo.md section 3.3 records.
    constexpr unsigned kMaxCatchUpFrames = 4;
    double guestFrameDebt = 0.0;
    Uint64 lastStepTicks = SDL_GetTicks();
#endif

    while (sms::IsRunning) {
        const AuroraEvent *event = aurora_update();
        while (event != nullptr && event->type != AURORA_NONE) {
            switch (event->type) {
                case AURORA_SDL_EVENT:
                    sms::ui::handle_event(event->sdl);
                    break;
                case AURORA_EXIT:
                    return true;
                default:
                    break;
            }
            event++;
        }

#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME
        // Do not present a frame the guest did not draw.
        //
        // aurora_begin_frame() clears the EFB, and everything the guest emits
        // is drained into it at end_frame. On a 144Hz display with a 60Hz
        // guest, most host iterations have no guest step behind them, so they
        // cleared the EFB and presented it empty - a black frame between every
        // pair of real ones. That is the flicker, and it is structural rather
        // than a glitch: it was predicted from the loop's shape before it was
        // ever seen.
        //
        // The debt is accumulated here, ahead of begin_frame, so the decision
        // can be made before anything is cleared. When no guest frame is due
        // the loop yields instead of drawing; the window keeps showing the
        // last complete image, which is what a real console does between
        // retraces.
        //
        // The menu-only path is unaffected: with no game running there is no
        // guest frame to wait for and every iteration presents as before.
        {
            const Uint64 nowTicks = SDL_GetTicks();
            guestFrameDebt += double(nowTicks - lastStepTicks) / 1000.0;
            lastStepTicks = nowTicks;
            if (guestFrameDebt > kMaxCatchUpFrames * sms::recomp::kGuestFrameSeconds) {
                guestFrameDebt = kMaxCatchUpFrames * sms::recomp::kGuestFrameSeconds;
            }
            if (g_gameRunning && guestFrameDebt < sms::recomp::kGuestFrameSeconds) {
                // Sleep rather than spin: this is the majority of iterations
                // on a high-refresh display, and burning a core on them would
                // take host time away from the guest.
                SDL_Delay(1);
                continue;
            }
        }
#endif

        const bool frameBegun = aurora_begin_frame();

#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME
        // Liveness heartbeat, deliberately *outside* the begin_frame guard and
        // the g_gameRunning check: a stall shows up as "no new log lines", and
        // that looks identical whether the guest is spinning, the guest has
        // halted, or the frame loop itself is the thing that stopped
        // (aurora_begin_frame() returning false skips the step entirely).
        // Counting frames and steps separately tells those three apart, and
        // the pc alongside them says *where* a spinning guest is spinning.
        //
        // Kept rather than removed: one line per second is bounded by
        // construction, and this has been the single most useful diagnostic in
        // the whole bring-up - every boot blocker so far was first located by
        // reading a pc out of this line.
        {
            static Uint64 lastHeartbeat = 0;
            static Uint64 lastHeartbeatStart = SDL_GetTicks();
            static u64 frames = 0;
            static u64 steps = 0;
            static u64 framesReported = 0;
            static u64 stepsReported = 0;
            ++frames;
            const Uint64 now = SDL_GetTicks();
            if (now - lastHeartbeat >= 1000) {
                // The window is this heartbeat's own span, not the time since
                // the process started - using the latter made the percentage
                // shrink towards zero as the run went on, which would have read
                // as "step_game is getting cheaper" when nothing had changed.
                const Uint64 heartbeatWindowMs = now - lastHeartbeatStart;
                lastHeartbeatStart = now;
                lastHeartbeat = now;
                // Deltas as well as totals: "is it still advancing, and how
                // fast" is a different question from "how far has it got", and
                // the totals alone make the first one arithmetic homework.
                static Uint64 stepTicksReported = 0;
                const double stepSeconds = double(g_stepTicks - stepTicksReported) / double(SDL_GetPerformanceFrequency());
                stepTicksReported = g_stepTicks;
                SmsMainLog.info("heartbeat: frames={} (+{}) steps={} (+{}) begun={} running={} pc={:#010x} lr={:#010x} downcount={} r3={:#010x} r4={:#010x} | step_game {:.0f}% of wall time",
                    frames, frames - framesReported, steps, steps - stepsReported, frameBegun, g_gameRunning,
                    g_gameCpu.pc, g_gameCpu.lr, g_gameCpu.downcount,
                    g_gameCpu.gpr[3], g_gameCpu.gpr[4],
                    heartbeatWindowMs == 0 ? 0.0 : 100.0 * stepSeconds * 1000.0 / double(heartbeatWindowMs));
                framesReported = frames;
                stepsReported = steps;
            }
            // The guest gets one step_game per *guest* frame, not one per host
            // frame. Those are not the same thing and treating them as the same
            // was making the game run at the display's refresh rate: measured
            // "+144" steps per second in this heartbeat on a 144Hz monitor,
            // while step_game advances the guest clock by exactly kCyclesPerFrame
            // = kCpuClockHz/60 each time (recomp_boot.cpp:757). The guest was
            // therefore living 2.4 seconds per real second - unplayable, and the
            // reason the AI audio path reports dropping queued bytes: it is being
            // handed 2.4x more samples than a 32kHz device can consume.
            //
            // So accumulate real elapsed time and run whole guest frames out of
            // it. The leftover stays in the accumulator, which keeps the long-run
            // rate exact instead of drifting by a fraction of a frame each time.
            //
            // NOTE: 60 is what the clock model already assumes, so this makes the
            // loop self-consistent with it. GMSP01 is the PAL disc and the real
            // retrace rate is whatever the guest programs into VI; deriving both
            // this and kCyclesPerFrame from the VI registers rather than from a
            // constant is a separate, unmeasured question and is NOT settled here.
            // (declared at function scope - see the pacing block above)
            // Never run more than a few guest frames per host frame. Without a
            // cap, one long hitch (a shader compile, a disc read) makes the next
            // iteration try to catch up over the whole gap, which takes even
            // longer and never recovers.

            // Where the host second actually goes. The port sits at ~101% of
            // one core, and "the guest is slow" and "the host loop around it is
            // expensive" look identical from outside the process: the render
            // loop runs at the display's 144Hz while only ~36 guest frames a
            // second produce new content, so every present and every FIFO drain
            // in between is host work with no guest work behind it. This says
            // which of the two it is instead of leaving it to be guessed at.
            // At most ONE guest frame per host iteration, even when behind.
            //
            // Running the catch-up as a loop meant up to four guest frames
            // back to back with no present between them. A guest frame costs
            // about 26ms of host time here, so that is up to 104ms of frozen
            // window: measured as the heartbeat's host frame count collapsing
            // to +9 per second while step_game took 93-94% of wall time. The
            // guest was fine; the picture was not.
            //
            // Stepping once per iteration lets the host present between guest
            // frames, so the window refreshes at least as often as the guest
            // produces content. The debt accumulator is unchanged and still
            // capped, so nothing is lost - the port simply stops trying to
            // catch up faster than it can draw.
            const Uint64 stepStart = SDL_GetPerformanceCounter();
            if (frameBegun && g_gameRunning && guestFrameDebt >= sms::recomp::kGuestFrameSeconds) {
                guestFrameDebt -= sms::recomp::kGuestFrameSeconds;
                ++steps;
                g_gameRunning = sms::recomp::step_game(&g_gameCpu, kGameBlocksPerFrame);
            }
            g_stepTicks += SDL_GetPerformanceCounter() - stepStart;
        }
#endif

        if (!frameBegun) {
            continue;
        }

        sms::ui::update();

        aurora_end_frame();
    }

    return true;
}

extern "C" int port_main(int argc, char *argv[])
{
    sms::registerSettings();
    sms::config::FinishRegistration();

    sms::ConfigPath = calculate_config_path();
    sms::config::LoadFromUserPreferences();

    AuroraConfig config {};
    config.appName = "DolphinJet";
    const auto configPathString = sms::ConfigPath.u8string();
    config.userPath = reinterpret_cast<const char *>(configPathString.c_str());
    config.vsync = sms::getSettings().video.enableVsync;
    config.startFullscreen = sms::getSettings().video.enableFullscreen;
    config.windowPosX = -1;
    config.windowPosY = -1;
    config.windowWidth = 640 * 2;
    config.windowHeight = 480 * 2;
    config.desiredBackend = resolve_desired_backend();
    config.logCallback = &aurora_log_callback;
    config.allowJoystickBackgroundEvents = sms::getSettings().game.allowBackgroundInput;
    config.pauseOnFocusLost = sms::getSettings().game.pauseOnFocusLost;
    config.mem1Size = 24 * 1024 * 1024;
    config.mem2Size = 16 * 1024 * 1024;

    const AuroraInfo auroraInfo = aurora_initialize(argc, argv, &config);

    VISetWindowTitle("DolphinJet");

    AuroraSetViewportPolicy(sms::getSettings().video.lockAspectRatio.getValue() ? AURORA_VIEWPORT_FIT : AURORA_VIEWPORT_STRETCH);

    if (!sms::ui::initialize()) {
        SmsMainLog.error("Failed to initialize the UI (RmlUi not ready), aborting");
        aurora_shutdown();
        return 1;
    }

    sms::ui::push_document(std::make_unique<sms::ui::Overlay>(), true, true);
    sms::ui::push_document(std::make_unique<sms::ui::MenuBar>(), false);
    (void)auroraInfo;

#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME
    try_boot_game();
#endif

    const bool cleanExit = run_menu_loop();

#ifdef DOLPHINJET_HAVE_RECOMPILED_GAME
    if (g_gameCpu.ram != nullptr) {
        cpu_free(&g_gameCpu);
    }
#endif

    sms::ui::shutdown();
    aurora_shutdown();

    return cleanExit ? 0 : 1;
}
