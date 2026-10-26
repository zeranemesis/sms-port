// Real SDLActivity + Aurora Vulkan integration, without game assets/runtime.
#include "sms_aurora.h"
#include "sms_aurora_audio.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <android/log.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int sms_gx_static_integration_test();
namespace {
std::atomic<unsigned> callbacks{0};
std::atomic<bool> badAudio{false};
void pcm(void*, unsigned char* output, int bytes) {
    if (bytes <= 0 || bytes > 2048 || bytes % 4) badAudio.store(true);
    if (bytes > 0 && bytes <= 2048) std::memset(output, 0, size_t(bytes));
    callbacks.fetch_add(1);
}
void report(const char* message) {
    __android_log_write(ANDROID_LOG_INFO, "SunshineSurface", message);
    if (const char* root = SDL_GetAndroidInternalStoragePath()) {
        const std::string path = std::string(root) + "/surface-result.txt";
        if (FILE* out = std::fopen(path.c_str(), "a")) {
            std::fprintf(out, "%s\n", message);
            std::fclose(out);
        }
    }
}
bool audioTest() {
    if (sab_audio_open(nullptr, nullptr)) return false;
    SabAudioHandle handle = sab_audio_open(pcm, nullptr);
    if (!handle) return false;
    SDL_Delay(100);
    const bool paused = callbacks.load() == 0;
    const bool started = sab_audio_start(handle) != 0;
    for (int i = 0; started && !callbacks.load() && i < 100; ++i) SDL_Delay(20);
    sab_audio_close(handle);
    const unsigned finished = callbacks.load();
    SDL_Delay(100);
    const bool joined = callbacks.load() == finished;
    // Close/start of an absent handle must be harmless, without dereferencing it.
    sab_audio_close(handle);
    const bool rejected = sab_audio_start(handle) == 0;
    SabAudioHandle second = sab_audio_open(pcm, nullptr);
    const bool secondStarted = second && sab_audio_start(second);
    sab_audio_close_all();
    const unsigned afterAll = callbacks.load();
    SDL_Delay(100);
    return paused && started && finished > 0 && joined && rejected && secondStarted
        && callbacks.load() == afterAll && !badAudio.load();
}
}

int main(int, char**) {
    report("START ARM64 real GX/FIFO, audio and Vulkan surface diagnostic; no game assets");
    if (sms_gx_static_integration_test()) { report("FAIL GX/FIFO"); return 1; }
    report("PASS real GX API/FIFO: texture pointers, getters, record/replay and BE arrays");
    if (!audioTest()) { report("FAIL SDL3 PCM lifecycle"); return 2; }
    report("PASS SDL3 PCM: paused open, 32kHz S16 stereo callbacks, bounded frames, close/join and close_all");
    const char* storage = SDL_GetAndroidInternalStoragePath();
    SabConfig config{};
    config.title = "Sunshine Aurora - test Vulkan";
    config.userPath = storage;
    config.windowWidth = 640; config.windowHeight = 480;
    config.backend = 4; config.vsync = 1; config.msaa = 1;
    config.anisotropy = 16; config.logLevel = 1;
    // Explicit diagnostic opt-in for software Vulkan emulators. Production
    // defaults keep rejecting CPU adapters; no renderer limits are reduced.
    setenv("SMS_AURORA_ALLOW_CPU_ADAPTER", "1", 1);
    if (!sab_init(&config)) { report("FAIL Aurora initialization"); return 3; }
    if (std::strcmp(sab_backend_name(), "Vulkan")) {
        report("FAIL unexpected graphics backend"); sab_shutdown(); return 4;
    }
    report("PASS Aurora initialized with Vulkan and SDL Android surface");
    unsigned frames = 0;
    const Uint64 start = SDL_GetTicks();
    bool quit = false;
    while (!quit && frames < 90 && SDL_GetTicks() - start < 30000) {
        SabEvent event;
        while (sab_poll_event(&event)) if (event.type == SAB_EV_QUIT) quit = true;
        if (!quit && sab_begin_frame()) { sab_end_frame(); ++frames; }
        SDL_Delay(10);
    }
    uint32_t width = 0, height = 0;
    sab_get_window_size(&width, &height);
    sab_shutdown();
    char result[256];
    const bool success = frames >= 10 && width && height;
    std::snprintf(result, sizeof result, "%s Vulkan surface: %u submitted frames, window %ux%u, shutdown returned; game rendering not tested",
                  success ? "PASS" : "FAIL", frames, width, height);
    report(result);
    return success ? 0 : 5;
}
