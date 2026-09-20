// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm

#ifndef SMS_PORT_SETTINGS_H
#define SMS_PORT_SETTINGS_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
#include "port/config_var.hpp"

namespace sms {

using namespace config;

// Matches dolphin/os.h's OS_LANGUAGE_* values; kept independent of that header
// so this file stays includable from code that only needs settings access.
enum class GameLanguage : u8 {
    English = 0,
    French = 2,
};

enum class DiscVerificationState : u8 {
    Unknown = 0,
    Success,
    HashMismatch,
};

namespace config {
template <>
struct ConfigEnumRange<GameLanguage> {
    // Bounds the raw stored value to OS_LANGUAGE_ENGLISH..OS_LANGUAGE_FRENCH,
    // the real SDK's valid encodings in that span (German=1 sits inside it
    // unused, same as Marioparty4's own range) rather than to just the two
    // languages settings.cpp's UI currently exposes a picker for.
    static constexpr auto min = GameLanguage::English;
    static constexpr auto max = GameLanguage::French;
};

template <>
struct ConfigEnumRange<DiscVerificationState> {
    static constexpr auto min = DiscVerificationState::Unknown;
    static constexpr auto max = DiscVerificationState::HashMismatch;
};
}

// Persistent user settings.
//
// This is a deliberately small starting set: only what the Aurora bootstrap
// and the Party-Board-style menu shell need to run before any SMS game code
// is linked in. Fields for gameplay options (cheats, audio) belong here once
// the corresponding systems exist; adding them speculatively now would just
// be dead settings with nothing to read them.
struct UserSettings {
    struct {
        ConfigVar<bool> enableFullscreen;
        ConfigVar<bool> enableVsync;
        ConfigVar<int> targetFrameRate;
        ConfigVar<bool> lockAspectRatio;
        ConfigVar<bool> enableAdaptiveWidescreen;
        ConfigVar<bool> enableFpsOverlay;
        ConfigVar<int> fpsOverlayCorner;
    } video;

    struct {
        ConfigVar<GameLanguage> language;
        ConfigVar<bool> pauseOnFocusLost;
        ConfigVar<bool> enableControllerToasts;
        ConfigVar<int> internalResolutionScale;
        ConfigVar<int> shadowResolutionMultiplier;
        ConfigVar<bool> allowBackgroundInput;
    } game;

    struct {
        ConfigVar<std::string> discPath;
        ConfigVar<DiscVerificationState> discVerification;
        ConfigVar<std::string> graphicsBackend;
        ConfigVar<bool> skipPreLaunchUI;
    } backend;
};

UserSettings& getSettings();

void registerSettings();

}
#endif // __cplusplus

#endif // SMS_PORT_SETTINGS_H
