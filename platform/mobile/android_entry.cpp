#if defined(__ANDROID__) && defined(SMS_ANDROID_APP)
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <android/log.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

int sms_android_host_main(int, char**);
namespace {
std::atomic<bool> started{false};
bool directory(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
bool regular(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0;
}
void environment(const char* key, const std::string& value) { setenv(key, value.c_str(), 1); }
}

// SDL invokes this on SDLThread, never the Android UI thread. Each session
// owns a separate :game process: console reset/exit ends that process, and a
// later launch gets fresh OS/thread/GX globals rather than unsafe reinit.
extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char** argv) {
    if (started.exchange(true)) return 1;
    const char* internal = SDL_GetAndroidInternalStoragePath();
    const char* external = SDL_GetAndroidExternalStoragePath();
    if (!internal || !external) return 2;
    const std::string root(internal), data(external);
    const int log = open((root + "/game-runtime.log").c_str(), O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (log >= 0) { dup2(log, STDERR_FILENO); dup2(log, STDOUT_FILENO); close(log); }
    setvbuf(stderr, nullptr, _IONBF, 0);
    std::fprintf(stderr, "[android] one session per game process, private configuration, real-time simulation\n");
    if (chdir(root.c_str()) != 0) return 3;
    environment("SMS_SETTINGS", root + "/settings.txt");
    environment("SMS_BINDINGS", root + "/bindings.txt");
    environment("SMS_SAVE_DIR", root + "/card-a");
    environment("SMS_AURORA_USER_PATH", root);
    environment("SMS_GX_STATS", "120");
    environment("XDG_DATA_HOME", root);
    environment("XDG_CACHE_HOME", root + "/cache");
    mkdir((root + "/card-a").c_str(), 0700);
    mkdir((root + "/cache").c_str(), 0700);
    // The emulator opts into a CPU Vulkan adapter; actual phones keep the
    // production default. This changes the test driver, never game quality.
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--android-menu")) environment("SMS_MENU_ON_START", "1");
        if (!std::strcmp(argv[i], "--android-cpu-adapter"))
            environment("SMS_AURORA_ALLOW_CPU_ADAPTER", "1");
        if (!std::strcmp(argv[i], "--android-capture")) {
            environment("SMS_ANDROID_INPUT_TRACE", "1");
            mkdir((root + "/captures").c_str(), 0700);
            environment("SMS_GX_DUMP_DIR", root + "/captures");
            environment("SMS_GX_DUMP_EVERY", "30");
        }
        if (!std::strcmp(argv[i], "--android-shader-cold-test")) {
            const std::string cache = root + "/cache/shader-test-" + std::to_string(getpid());
            mkdir(cache.c_str(), 0700);
            environment("SMS_AURORA_CACHE_PATH", cache);
        }
        if (!std::strcmp(argv[i], "--android-surface-reference"))
            environment("SMS_ANDROID_SURFACE_LOCK_REFERENCE", "1");
        if (!std::strcmp(argv[i], "--android-fifo-reference"))
            environment("SMS_NATIVE_FIFO_REFERENCE", "1");
        if (!std::strcmp(argv[i], "--android-performance-profile")) {
            environment("SMS_GX_FRAMELOG", root + "/perf-frames.log");
            environment("SMS_OS_PROFILE", root + "/perf-scheduler.log");
            environment("SMS_AURORA_FRAME_PROFILE", root + "/perf-render-frames.log");
            environment("SMS_AURORA_BEGIN_PROFILE", root + "/perf-begin.log");
            environment("SMS_AURORA_WORKER_PROFILE", root + "/perf-worker.log");
            environment("SMS_AURORA_PIPELINE_PROFILE", root + "/perf-pipelines.log");
            environment("SMS_AURORA_PEEK_PROFILE", root + "/perf-peeks.log");
        }
        if (!std::strcmp(argv[i], "--android-stage-tour")) {
            // Diagnostic route starts in Bianco Hills rather than the arrival movie.
            environment("SMS_STAGE_TOUR", "2,0");
            environment("SMS_STAGE_TOUR_FRAMES", "1800");
        }
    }
    // Only explicit diagnostic launches can select a stage or test duration.
    if (std::getenv("SMS_STAGE_TOUR")) {
        int stage = 2, scenario = 0, frames = 1800;
        for (int i = 1; i < argc; ++i) {
            char tail;
            int value;
            if (std::sscanf(argv[i], "--android-stage=%d%c", &value, &tail)==1 && value>=0 && value<=10) stage=value;
            if (std::sscanf(argv[i], "--android-scenario=%d%c", &value, &tail)==1 && value>=0 && value<=8) scenario=value;
            if (std::sscanf(argv[i], "--android-stage-frames=%d%c", &value, &tail)==1 && value>=300 && value<=36000) frames=value;
        }
        environment("SMS_STAGE_TOUR", std::to_string(stage)+","+std::to_string(scenario));
        environment("SMS_STAGE_TOUR_FRAMES", std::to_string(frames));
    }
    const std::string iso = data + "/game.iso", files = data + "/game/files";
    if (regular(iso)) environment("SMS_DISC_IMAGE", iso);
    else if (directory(files)) environment("SMS_DISC_ROOT", files);
    else {
        std::fprintf(stderr, "[android] missing game.iso or game/files in app data\n");
        return 4;
    }
    // Pass only owned platform switches; private filesystem paths are explicit
    // environment configuration, not transient JNI strings or content URIs.
    char name[] = "SunshineAndroid", noLauncher[] = "--no-launcher";
    char* gameArgs[] = {name, noLauncher, nullptr};
    const int status = sms_android_host_main(2, gameArgs);
    std::fprintf(stderr, "[android] game worker returned %d\n", status);
    // No attempt to restart the process-global console OS in this JVM.
    std::exit(status);
}
#endif
