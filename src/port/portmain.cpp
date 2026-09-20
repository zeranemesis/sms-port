// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm
//
// This is the Aurora bootstrap for the SMS native PC port: it brings up a
// window, the Party-Board-style menu (F1), and an update/render loop, but
// does not yet call into any SMS game code. There is none linked in yet -
// see docs/port_bootstrap.md for what still has to happen (recompiled or
// decompiled game code providing a game_main-equivalent entry point) before
// this loop has an actual game to hand off to.
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

#include <cstdio>
#include <cstdlib>

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
        std::fflush(out);
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

// Runs the Aurora event/render loop with only the Party-Board-style menu on
// screen. Once SMS game code (recompiled and/or decompiled, per
// docs/recompilation.md) provides an entry point, this becomes the fallback
// path for BACKEND_NULL / no-disc-configured, the same way Marioparty4's
// launchUILoop() is, rather than the whole program.
//
// Both ways out below - the Quit menu action (sets sms::IsRunning to
// false) and AURORA_EXIT (the OS/window asking to close) - are ordinary,
// successful ways to end the program, not errors; there is no failure path
// in this loop yet. The bool return stays meaningful for later, once a
// real error condition exists to report through it.
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

        if (!aurora_begin_frame()) {
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
    // generated/ exists (see CMakeLists.txt), so game_recompiled built
    // successfully - but booting it needs two more things this comment
    // can't invent: the entry point's guest address, and the real
    // DOLRECOMP_SYMBOL_* bindings for sms::recomp::dolphin_sdk's
    // trampolines. Both come from generated/generated_symbols.h, which is
    // specific to whatever GMSP01 dump produced generated/ and isn't
    // something this codebase can know in advance. The shape once that
    // header exists: #include it here, build a CPUState (cpu_init),
    // sms::recomp::install_host_calls(&cpu), pass its DOLRECOMP_SYMBOL_*
    // constants to sms::recomp::dolphin_sdk::register_known_dolphin_sdk_calls,
    // set cpu.pc to the entry symbol, and drive func_<entry>(&cpu) from
    // run_menu_loop()'s per-frame update instead of just the menu.
#endif

    const bool cleanExit = run_menu_loop();

    sms::ui::shutdown();
    aurora_shutdown();

    return cleanExit ? 0 : 1;
}
