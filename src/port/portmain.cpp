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
constexpr unsigned kGameBlocksPerFrame = 4096;

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
            static u64 frames = 0;
            static u64 steps = 0;
            static u64 framesReported = 0;
            static u64 stepsReported = 0;
            ++frames;
            const Uint64 now = SDL_GetTicks();
            if (now - lastHeartbeat >= 1000) {
                lastHeartbeat = now;
                // Deltas as well as totals: "is it still advancing, and how
                // fast" is a different question from "how far has it got", and
                // the totals alone make the first one arithmetic homework.
                SmsMainLog.info("heartbeat: frames={} (+{}) steps={} (+{}) begun={} running={} pc={:#010x} lr={:#010x} downcount={} r3={:#010x} r4={:#010x}",
                    frames, frames - framesReported, steps, steps - stepsReported, frameBegun, g_gameRunning,
                    g_gameCpu.pc, g_gameCpu.lr, g_gameCpu.downcount,
                    g_gameCpu.gpr[3], g_gameCpu.gpr[4]);
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
            constexpr double kGuestFrameSeconds = 1.0 / 60.0;
            // Never run more than a few guest frames per host frame. Without a
            // cap, one long hitch (a shader compile, a disc read) makes the next
            // iteration try to catch up over the whole gap, which takes even
            // longer and never recovers.
            constexpr unsigned kMaxCatchUpFrames = 4;
            static double guestFrameDebt = 0.0;
            static Uint64 lastStepTicks = SDL_GetTicks();

            const Uint64 nowTicks = SDL_GetTicks();
            guestFrameDebt += double(nowTicks - lastStepTicks) / 1000.0;
            lastStepTicks = nowTicks;
            if (guestFrameDebt > kMaxCatchUpFrames * kGuestFrameSeconds) {
                guestFrameDebt = kMaxCatchUpFrames * kGuestFrameSeconds;
            }

            while (frameBegun && g_gameRunning && guestFrameDebt >= kGuestFrameSeconds) {
                guestFrameDebt -= kGuestFrameSeconds;
                ++steps;
                g_gameRunning = sms::recomp::step_game(&g_gameCpu, kGameBlocksPerFrame);
            }
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
