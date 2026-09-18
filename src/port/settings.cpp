// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm
#include "port/settings.h"
#include "port/config.hpp"

namespace sms {

UserSettings g_userSettings = {
    .video = {
        .enableFullscreen {"video.enableFullscreen", false},
        .enableVsync {"video.enableVsync", true},
        .targetFrameRate {"video.targetFrameRate", 60},
        .lockAspectRatio {"video.lockAspectRatio", false},
        .enableAdaptiveWidescreen {"video.enableAdaptiveWidescreen", false},
        .enableFpsOverlay {"video.enableFpsOverlay", false},
        .fpsOverlayCorner {"video.fpsOverlayCorner", 0},
    },

    .game = {
        .language { "game.language", GameLanguage::English },
        .pauseOnFocusLost {"game.pauseOnFocusLost", false},
        .enableControllerToasts {"game.enableControllerToasts", true},
        .internalResolutionScale {"game.internalResolutionScale", 0},
        .shadowResolutionMultiplier {"game.shadowResolutionMultiplier", 1},
        .allowBackgroundInput {"game.allowBackgroundInput", true},
    },

    .backend = {
        .discPath {"backend.discPath", ""},
        .discVerification {"backend.discVerification", DiscVerificationState::Unknown},
        .graphicsBackend {"backend.graphicsBackend", "auto"},
        .skipPreLaunchUI {"backend.skipPreLaunchUI", false},
    }
};

UserSettings& getSettings() {
    return g_userSettings;
}

void registerSettings() {
    Register(g_userSettings.video.enableFullscreen);
    Register(g_userSettings.video.enableVsync);
    Register(g_userSettings.video.targetFrameRate);
    Register(g_userSettings.video.lockAspectRatio);
    Register(g_userSettings.video.enableAdaptiveWidescreen);
    Register(g_userSettings.video.enableFpsOverlay);
    Register(g_userSettings.video.fpsOverlayCorner);

    Register(g_userSettings.game.language);
    Register(g_userSettings.game.pauseOnFocusLost);
    Register(g_userSettings.game.enableControllerToasts);
    Register(g_userSettings.game.internalResolutionScale);
    Register(g_userSettings.game.shadowResolutionMultiplier);
    Register(g_userSettings.game.allowBackgroundInput);

    Register(g_userSettings.backend.discPath);
    Register(g_userSettings.backend.discVerification);
    Register(g_userSettings.backend.graphicsBackend);
    Register(g_userSettings.backend.skipPreLaunchUI);
}

}
