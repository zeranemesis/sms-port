// Window / context bring-up for sms_gx.
//
// Default: an SDL2 window with an OpenGL 3.3 core context whenever a display
// is available.  Headless (tests, CI, no display): an EGL context without a
// surface.  GXInit brings this up automatically if the host has not called
// GXPC_Init itself, and every GXCopyDisp presents the XFB to the window.
#include "gx_internal.h"
#include "gx_window_layout.h"
#include "gx_live_settings.h"
#include "sms_gx/gx_pc.h"
extern "C" void GXPC_MenuShutdown(void);

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>

#ifdef SMS_GX_HAVE_SDL2
#include <SDL.h>
#endif
#ifdef SMS_GX_HAVE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

namespace gx {
extern void (*g_displayCopyHook)(const void* xfb);
bool rendererReady();
extern double g_presentSeconds, g_swapSeconds;
}

using namespace gx;

namespace {
enum Mode { MODE_NONE, MODE_WINDOW, MODE_HEADLESS, MODE_EXTERNAL };
Mode s_mode = MODE_NONE;
int s_forceHeadless = -1;  // -1: decide from the environment
bool s_autoPresent = true;
int s_vsync = 0;
uint32_t s_frame = 0;

#ifdef SMS_GX_HAVE_SDL2
SDL_Window* s_window = nullptr;
SDL_GLContext s_glctx = nullptr;
void (*s_eventCb)(const SDL_Event*) = nullptr;
std::vector<SDL_GameController*> s_pads;
#endif
std::vector<uint8_t> s_icon;  // GXPC_SetWindowIcon, RGBA8
int s_iconW = 0, s_iconH = 0;

double nowSeconds() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
}

bool envTrue(const char* name) {
    const char* v = getenv(name);
    return v && *v && strcmp(v, "0") != 0;
}

#ifdef SMS_GX_HAVE_SDL2
void* sdlGetProc(const char* name) { return SDL_GL_GetProcAddress(name); }

// SMS_WINDOW_MODE: windowed (default), borderless (fullscreen at the desktop's
// resolution) or fullscreen (exclusive, at SMS_FULLSCREEN_MODE=WxH[@Hz] or the
// desktop's mode). F11 or Alt+Enter toggles between the window and the
// fullscreen mode chosen (borderless when the window mode is windowed).
enum WindowMode { WM_WINDOWED, WM_BORDERLESS, WM_FULLSCREEN };
WindowMode s_fullscreenKind = WM_BORDERLESS;
bool s_isFullscreen = false;
WindowMode s_configuredWindowMode = WM_WINDOWED;
int s_configuredDisplay = -1, s_configuredWindowScale = 0;
std::string s_configuredFullscreenMode = "desktop";

WindowMode parseWindowMode(const char* e) {
    if (!e) return WM_WINDOWED;
    if (!strcmp(e, "borderless")) return WM_BORDERLESS;
    if (!strcmp(e, "fullscreen") || !strcmp(e, "exclusive")) return WM_FULLSCREEN;
    return WM_WINDOWED;
}

void setFullscreen(bool on) {
    if (!s_window) return;
    Uint32 flags = 0;
    if (on && s_fullscreenKind == WM_FULLSCREEN) {
        SDL_DisplayMode want = {}, got = {};
        const int display = std::max(0, SDL_GetWindowDisplayIndex(s_window));
        SDL_GetDesktopDisplayMode(display, &want);
        if (const char* e = getenv("SMS_FULLSCREEN_MODE")) {
            int w = 0, h = 0, hz = 0;
            if (sscanf(e, "%dx%d@%d", &w, &h, &hz) >= 2 && w > 0 && h > 0) {
                want.w = w;
                want.h = h;
                if (hz > 0) want.refresh_rate = hz;
            }
        }
        if (SDL_GetClosestDisplayMode(display, &want, &got)) SDL_SetWindowDisplayMode(s_window, &got);
        flags = SDL_WINDOW_FULLSCREEN;
        logmsg("exclusive fullscreen %dx%d@%dHz", got.w, got.h, got.refresh_rate);
    } else if (on) {
        flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    if (SDL_SetWindowFullscreen(s_window, flags) != 0) {
        logmsg("fullscreen change failed: %s", SDL_GetError());
        return;
    }
    s_isFullscreen = on;
    SDL_ShowCursor(on ? SDL_DISABLE : SDL_ENABLE);
}

// SMS_MOUSE_CAMERA=1: mouse look. The mouse is captured (relative mode) while
// the window has focus; F10 releases it, a click in the window takes it back,
// and losing focus always frees it.
bool s_mouseCamera = false, s_mouseCaptured = false, s_mouseReleased = false;
bool s_menuWasVisible = false;

void captureMouse(bool on) {
    if (!s_mouseCamera) on = false;
    if (on == s_mouseCaptured) return;
    if (SDL_SetRelativeMouseMode(on ? SDL_TRUE : SDL_FALSE) == 0) s_mouseCaptured = on;
}

// The PAD bridge remembers event state. Clear it on both transitions so menu
// navigation cannot leave Mario walking, jumping or holding a trigger.
void syncMenuInput() {
    const bool visible = GXPC_MenuVisible() != 0;
    if (visible == s_menuWasVisible) return;
    s_menuWasVisible = visible;
    if (s_eventCb) {
        SDL_Event release = {};
        release.type = SDL_KEYUP;
        release.key.state = SDL_RELEASED;
        for (int key = 0; key < SDL_NUM_SCANCODES; ++key) {
            release.key.keysym.scancode = SDL_Scancode(key);
            s_eventCb(&release);
        }
        release = {};
        release.type = SDL_CONTROLLERBUTTONUP;
        release.cbutton.state = SDL_RELEASED;
        for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; ++button) {
            release.cbutton.button = Uint8(button);
            s_eventCb(&release);
        }
        release = {};
        release.type = SDL_CONTROLLERAXISMOTION;
        for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis) {
            release.caxis.axis = Uint8(axis);
            s_eventCb(&release);
        }
    }
    if (visible) {
        captureMouse(false);
        SDL_ShowCursor(SDL_ENABLE);
    } else {
        captureMouse(!s_mouseReleased &&
                     (SDL_GetWindowFlags(s_window) & SDL_WINDOW_INPUT_FOCUS) != 0);
        SDL_ShowCursor(s_isFullscreen && !s_mouseCaptured ? SDL_DISABLE : SDL_ENABLE);
    }
}

void applyIcon() {
    if (!s_window || s_icon.empty()) return;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(s_icon.data(), s_iconW, s_iconH, 32, s_iconW * 4,
                                                        SDL_PIXELFORMAT_RGBA32);
    if (!s) return;
    SDL_SetWindowIcon(s_window, s);
    SDL_FreeSurface(s);
}

bool openWindow(int scale) {
#ifdef SDL_HINT_WINDOWS_DPI_SCALING
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        logmsg("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    // Open on the monitor containing the pointer (usually the launcher's Play
    // button). Fall back to the primary display if global coordinates are unavailable.
    SDL_Point pointer = {};
    SDL_GetGlobalMouseState(&pointer.x, &pointer.y);
    int display = 0;
    for (int i = 0; i < SDL_GetNumVideoDisplays(); ++i) {
        SDL_Rect bounds;
        if (SDL_GetDisplayBounds(i, &bounds) == 0 && SDL_PointInRect(&pointer, &bounds)) {
            display = i;
            break;
        }
    }
    // SMS_DISPLAY=n picks the monitor (0 is the primary one)
    if (const char* e = getenv("SMS_DISPLAY")) {
        const int want = atoi(e);
        if (*e && want >= 0 && want < SDL_GetNumVideoDisplays()) display = want;
    }
    SDL_Rect desktop = {0, 0, 1280, 800};
    if (SDL_GetDisplayUsableBounds(display, &desktop) != 0 &&
        SDL_GetDisplayBounds(display, &desktop) != 0) desktop = {0, 0, 1280, 800};
    int windowScale = 0;
    if (const char* e = getenv("SMS_WINDOW_SCALE")) {
        const long requested = strtol(e, nullptr, 10);
        if (requested > 0) windowScale = int(std::min(requested, 16L));
    }
    const WindowArea layout = initialWindowLayout({desktop.x, desktop.y, desktop.w, desktop.h},
                                                  640.0 * GXPC_GetWidescreen() / 480.0, windowScale);
    s_window = SDL_CreateWindow("Super Mario Sunshine", layout.x, layout.y, layout.w, layout.h,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                                SDL_WINDOW_HIDDEN);
    if (!s_window) {
        logmsg("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return false;
    }
    SDL_SetWindowMinimumSize(s_window, std::min(320, layout.w), std::min(240, layout.h));
    applyIcon();
    s_glctx = SDL_GL_CreateContext(s_window);
    if (!s_glctx) {
        logmsg("OpenGL 3.3 core context failed: %s", SDL_GetError());
        SDL_DestroyWindow(s_window);
        s_window = nullptr;
        SDL_Quit();
        return false;
    }
    SDL_GL_MakeCurrent(s_window, s_glctx);
    // adaptive vsync (-1) tears only when a frame is late; not every driver has it
    if (SDL_GL_SetSwapInterval(s_vsync) != 0 && s_vsync < 0) SDL_GL_SetSwapInterval(1);
    if (!GXPC_Init(sdlGetProc, scale)) {
        SDL_GL_DeleteContext(s_glctx);
        SDL_DestroyWindow(s_window);
        s_glctx = nullptr;
        s_window = nullptr;
        SDL_Quit();
        return false;
    }
    GXPC_MenuInit(s_window, s_glctx);
    SDL_ShowWindow(s_window);
    // Some window managers choose their own placement when mapping a hidden
    // window. Center the decorated frame after showing it, using a conservative
    // title-bar allowance if the platform cannot report its borders yet.
    WindowBorders borders = {48, 8, 8, 8};
    if (SDL_GetWindowBordersSize(s_window, &borders.top, &borders.left, &borders.bottom, &borders.right) != 0)
        borders = {48, 8, 8, 8};
    SDL_SetWindowPosition(s_window,
        desktop.x + (desktop.w - layout.w - borders.left - borders.right) / 2 + borders.left,
        desktop.y + (desktop.h - layout.h - borders.top - borders.bottom) / 2 + borders.top);
    logmsg("window %dx%d centered on display %d, internal resolution scale %d, OpenGL context ready",
           layout.w, layout.h, display, scale);
    const WindowMode mode = parseWindowMode(getenv("SMS_WINDOW_MODE"));
    s_configuredWindowMode = mode;
    s_configuredDisplay = display;
    s_configuredWindowScale = windowScale;
    if (const char* resolution = getenv("SMS_FULLSCREEN_MODE"))
        s_configuredFullscreenMode = resolution;
    if (mode != WM_WINDOWED) {
        s_fullscreenKind = mode;
        setFullscreen(true);
    }
    s_mouseCamera = envTrue("SMS_MOUSE_CAMERA");
    if (s_mouseCamera) {
        logmsg("mouse look on (F10 releases the mouse)");
        captureMouse((SDL_GetWindowFlags(s_window) & SDL_WINDOW_INPUT_FOCUS) != 0);
    }
    return true;
}
#endif

#ifdef SMS_GX_HAVE_EGL
void* eglGetProc(const char* name) { return reinterpret_cast<void*>(eglGetProcAddress(name)); }

bool tryEglDisplay(EGLDisplay dpy) {
    EGLint major, minor;
    if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) return false;
    if (!eglBindAPI(EGL_OPENGL_API)) return false;
    const EGLint cfgAttr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE};
    EGLConfig cfg;
    EGLint n = 0;
    if (!eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n) || n < 1) return false;
    const EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
                              EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (ctx == EGL_NO_CONTEXT) return false;
    if (eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) return true;
    // no surfaceless support: fall back to a tiny pbuffer
    const EGLint pbAttr[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    EGLSurface pb = eglCreatePbufferSurface(dpy, cfg, pbAttr);
    return pb != EGL_NO_SURFACE && eglMakeCurrent(dpy, pb, pb, ctx);
}

bool openHeadless(int scale) {
    auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    auto queryDevices = reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
    bool ok = false;
    // Mesa crashes initialising a hardware EGL device when software rendering
    // is forced; go straight to the surfaceless platform then.
    if (getPlatformDisplay && queryDevices && !envTrue("LIBGL_ALWAYS_SOFTWARE")) {
        EGLDeviceEXT devs[8];
        EGLint n = 0;
        if (queryDevices(8, devs, &n))
            for (EGLint i = 0; i < n && !ok; i++) ok = tryEglDisplay(getPlatformDisplay(EGL_PLATFORM_DEVICE_EXT, devs[i], nullptr));
    }
    if (!ok && getPlatformDisplay)
        ok = tryEglDisplay(getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr));
    if (!ok) ok = tryEglDisplay(eglGetDisplay(EGL_DEFAULT_DISPLAY));
    if (!ok) {
        logmsg("headless: no EGL OpenGL 3.3 context (try LIBGL_ALWAYS_SOFTWARE=1)");
        return false;
    }
    return GXPC_Init(eglGetProc, scale) != 0;
}
#endif

void dumpFrame(const void* xfb) {
    static int every = -1;
    static const char* dir = nullptr;
    if (every < 0) {
        const char* e = getenv("SMS_GX_DUMP_EVERY");
        every = e ? atoi(e) : 0;
        dir = getenv("SMS_GX_DUMP_DIR");
        if (!dir) dir = ".";
    }
    if (every <= 0 || s_frame % uint32_t(every) != 0) return;
    int w = 0, h = 0;
    if (!GXPC_ReadXFB(xfb, nullptr, &w, &h)) return;
    std::vector<uint8_t> px(size_t(w) * h * 4);
    GXPC_ReadXFB(xfb, px.data(), &w, &h);
    char path[1024];
    snprintf(path, sizeof path, "%s/frame_%06u.ppm", dir, s_frame);
    FILE* f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) fwrite(&px[size_t(i) * 4], 1, 3, f);
    fclose(f);
}

void onDisplayCopy(const void* xfb) {
    s_frame++;
    dumpFrame(xfb);
    if (s_autoPresent) GXPC_Present(xfb);
}
}  // namespace

extern "C" {

void GXPC_ApplyMenuWindowSettings(const char* vsync, const char* windowMode,
                                const char* fullscreenMode, int display,
                                int windowScale, int mouseCamera) {
#ifdef SMS_GX_HAVE_SDL2
    if (!s_window || !s_glctx) return;
    const int requestedVsync = vsync && !strcmp(vsync, "adaptive") ? -1 :
                               vsync && (!strcmp(vsync, "on") || !strcmp(vsync, "1")) ? 1 : 0;
    int appliedVsync = requestedVsync;
    if (SDL_GL_SetSwapInterval(requestedVsync) != 0) {
        if (requestedVsync != -1 || SDL_GL_SetSwapInterval(1) != 0) {
            logmsg("live vsync change failed: %s", SDL_GetError());
            appliedVsync = s_vsync;
        } else appliedVsync = 1;
    }
    s_vsync = appliedVsync;
    const WindowMode requestedMode = windowMode ? parseWindowMode(windowMode) : s_configuredWindowMode;
    const std::string resolution = fullscreenMode && *fullscreenMode ? fullscreenMode : s_configuredFullscreenMode;
    const bool modeChanged = requestedMode != s_configuredWindowMode;
    const bool resolutionChanged = resolution != s_configuredFullscreenMode;
    const bool displayChanged = display >= 0 && display < SDL_GetNumVideoDisplays() &&
                                display != s_configuredDisplay;
    const bool sizeChanged = windowScale >= 0 && windowScale <= 16 &&
                             windowScale != s_configuredWindowScale;
    const bool restoreFullscreen = modeChanged ? requestedMode != WM_WINDOWED : s_isFullscreen;
    if (s_isFullscreen && (modeChanged || resolutionChanged || displayChanged || sizeChanged)) setFullscreen(false);
#ifdef _WIN32
    _putenv_s("SMS_FULLSCREEN_MODE", resolution.c_str());
    _putenv_s("SMS_WINDOW_MODE", requestedMode == WM_FULLSCREEN ? "fullscreen" :
                                  requestedMode == WM_BORDERLESS ? "borderless" : "windowed");
    _putenv_s("SMS_VSYNC", appliedVsync == -1 ? "adaptive" : appliedVsync ? "1" : "0");
    _putenv_s("SMS_MOUSE_CAMERA", mouseCamera ? "1" : "0");
#else
    setenv("SMS_FULLSCREEN_MODE", resolution.c_str(), 1);
    setenv("SMS_WINDOW_MODE", requestedMode == WM_FULLSCREEN ? "fullscreen" :
                              requestedMode == WM_BORDERLESS ? "borderless" : "windowed", 1);
    setenv("SMS_VSYNC", appliedVsync == -1 ? "adaptive" : appliedVsync ? "1" : "0", 1);
    setenv("SMS_MOUSE_CAMERA", mouseCamera ? "1" : "0", 1);
#endif
    if (displayChanged) s_configuredDisplay = display;
    if (sizeChanged) s_configuredWindowScale = windowScale;
    if (displayChanged || sizeChanged) {
        SDL_Rect desktop = {0, 0, 1280, 800};
        const int targetDisplay = std::max(0, s_configuredDisplay);
        if (SDL_GetDisplayUsableBounds(targetDisplay, &desktop) != 0)
            SDL_GetDisplayBounds(targetDisplay, &desktop);
        const WindowArea layout = initialWindowLayout({desktop.x, desktop.y, desktop.w, desktop.h},
                                                      640.0 * GXPC_GetWidescreen() / 480.0,
                                                      s_configuredWindowScale);
        SDL_SetWindowPosition(s_window, layout.x, layout.y);
        if (!s_isFullscreen) SDL_SetWindowSize(s_window, layout.w, layout.h);
    }
    s_configuredWindowMode = requestedMode;
    s_configuredFullscreenMode = resolution;
    if (requestedMode != WM_WINDOWED) s_fullscreenKind = requestedMode;
    if (restoreFullscreen && !s_isFullscreen) setFullscreen(true);
    const bool mouseEnabled = mouseCamera != 0;
    if (s_mouseCamera != mouseEnabled) {
        captureMouse(false);
        s_mouseCamera = mouseEnabled;
        s_mouseReleased = false;
    }
    captureMouse(mouseEnabled && !GXPC_MenuVisible() && !s_mouseReleased &&
                 (SDL_GetWindowFlags(s_window) & SDL_WINDOW_INPUT_FOCUS) != 0);
    logmsg("live menu settings applied: vsync %d, window mode %d, mouse look %s",
           s_vsync, int(requestedMode), mouseEnabled ? "on" : "off");
#else
    (void)vsync; (void)windowMode; (void)fullscreenMode;
    (void)display; (void)windowScale; (void)mouseCamera;
#endif
}

int GXPC_ParseArgs(int* argc, char** argv) {
    int out = 1;
    for (int i = 1; i < *argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "--headless") == 0) s_forceHeadless = 1;
        else if (strcmp(a, "--window") == 0) s_forceHeadless = 0;
        else if (strcmp(a, "--vsync") == 0) s_vsync = 1;
        else {
            argv[out++] = argv[i];
            continue;
        }
    }
    int removed = *argc - out;
    *argc = out;
    argv[out] = nullptr;
    return removed;
}

void GXPC_SetHeadless(int headless) { s_forceHeadless = headless ? 1 : 0; }

void GXPC_SetWindowIcon(const uint8_t* rgba, int w, int h) {
    if (!rgba || w <= 0 || h <= 0) return;
    s_icon.assign(rgba, rgba + (size_t)w * h * 4);
    s_iconW = w;
    s_iconH = h;
#ifdef SMS_GX_HAVE_SDL2
    applyIcon();
#endif
}
void GXPC_SetAutoPresent(int enable) { s_autoPresent = enable != 0; }
int GXPC_MouseCaptured(void) {
#ifdef SMS_GX_HAVE_SDL2
    return s_mouseCaptured;
#else
    return 0;
#endif
}
int GXPC_IsHeadless(void) { return s_mode != MODE_WINDOW; }
uint32_t GXPC_FrameCount(void) { return s_frame; }

int GXPC_InitAuto(int efbScale) {
    if (rendererReady()) return 1;
    if (const char* e = getenv("SMS_GX_SCALE")) efbScale = atoi(e) > 0 ? atoi(e) : efbScale;
    if (envTrue("SMS_VSYNC")) s_vsync = strcmp(getenv("SMS_VSYNC"), "adaptive") == 0 ? -1 : 1;
    bool headless;
    if (s_forceHeadless >= 0) headless = s_forceHeadless != 0;
    else if (envTrue("SMS_HEADLESS")) headless = true;
    else {
        const char* d = getenv("DISPLAY");
        const char* w = getenv("WAYLAND_DISPLAY");
        headless = !(d && *d) && !(w && *w);
#if defined(_WIN32) || defined(__APPLE__)
        headless = false;
#endif
    }
    g_displayCopyHook = onDisplayCopy;
#ifdef SMS_GX_HAVE_SDL2
    if (!headless) {
        if (openWindow(efbScale)) {
            s_mode = MODE_WINDOW;
            return 1;
        }
        logmsg("no window available, continuing headless");
    }
#endif
#ifdef SMS_GX_HAVE_EGL
    if (openHeadless(efbScale)) {
        s_mode = MODE_HEADLESS;
        logmsg("headless OpenGL context ready");
        return 1;
    }
#endif
    (void)headless;
    logmsg("no OpenGL context could be created; rendering is disabled");
    return 0;
}

void GXPC_Present(const void* xfb) {
#ifdef SMS_GX_HAVE_SDL2
    if (s_mode == MODE_WINDOW && s_window) {
        int w = 0, h = 0;
        SDL_GL_GetDrawableSize(s_window, &w, &h);
        double t0 = nowSeconds();
        GXPC_PresentXFB(xfb, w, h);
        GXPC_OverlayDraw(w, h);
        GXPC_MenuDraw(w, h);
        syncMenuInput();
        double t1 = nowSeconds();
        SDL_GL_SwapWindow(s_window);
        double t2 = nowSeconds();
        g_presentSeconds += t1 - t0;
        g_swapSeconds += t2 - t1;
        GXPC_EndPresent();
        sms_gx_pump_events();
        // Stop at the presentation boundary: the game cannot mutate its state
        // while the menu is open. Reuse the copied XFB rather than drawing game
        // actors again, and keep pumping events so the menu remains responsive.
        while (GXPC_MenuVisible()) {
            SDL_GL_GetDrawableSize(s_window, &w, &h);
            GXPC_PresentXFB(xfb, w, h);
            GXPC_MenuDraw(w, h);
            syncMenuInput();
            SDL_GL_SwapWindow(s_window);
            GXPC_EndPresent();
            sms_gx_pump_events();
            SDL_Delay(16);
        }
    }
#else
    (void)xfb;
#endif
}

void sms_gx_set_event_callback(void (*cb)(const union SDL_Event*)) {
#ifdef SMS_GX_HAVE_SDL2
    s_eventCb = cb;
#else
    (void)cb;
#endif
}

void sms_gx_pump_events(void) {
#ifdef SMS_GX_HAVE_SDL2
    if (s_mode != MODE_WINDOW) return;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_CONTROLLERDEVICEADDED:
            if (SDL_IsGameController(ev.cdevice.which)) {
                if (SDL_GameController* c = SDL_GameControllerOpen(ev.cdevice.which)) s_pads.push_back(c);
            }
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            for (size_t i = 0; i < s_pads.size(); i++)
                if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(s_pads[i])) == ev.cdevice.which) {
                    SDL_GameControllerClose(s_pads[i]);
                    s_pads.erase(s_pads.begin() + long(i));
                    break;
                }
            break;
        default:
            break;
        }
        // F1 (or the controller Guide button) opens the native settings menu.
        // Reserve the complete key pair so it never reaches game bindings.
        if (((ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) &&
             ev.key.keysym.scancode == SDL_SCANCODE_F1) ||
            ((ev.type == SDL_CONTROLLERBUTTONDOWN || ev.type == SDL_CONTROLLERBUTTONUP) &&
             ev.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE)) {
            if ((ev.type == SDL_KEYDOWN && !ev.key.repeat) || ev.type == SDL_CONTROLLERBUTTONDOWN) {
                GXPC_MenuToggle();
                syncMenuInput();
            }
            continue;
        }
        const bool menuWasVisible = GXPC_MenuVisible() != 0;
        const bool menuCaptured = GXPC_MenuProcessEvent(&ev) != 0;
        syncMenuInput();
        // Window close remains available even when a menu captures inputs.
        if (ev.type == SDL_QUIT ||
            (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_CLOSE)) {
            logmsg("window closed, exiting");
            GXPC_MenuShutdown();
            GXPC_Shutdown();
            exit(0);
        }
        if (menuWasVisible || GXPC_MenuVisible() || menuCaptured) continue;
        // backtick toggles the debug overlay and is kept from the pad layer
        if ((ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) && ev.key.keysym.scancode == SDL_SCANCODE_GRAVE) {
            if (ev.type == SDL_KEYDOWN && !ev.key.repeat) GXPC_OverlayToggle();
            continue;
        }
        if (s_mouseCamera) {
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) captureMouse(false);
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED && !s_mouseReleased)
                captureMouse(true);
            if (ev.type == SDL_MOUSEBUTTONDOWN && !s_mouseCaptured) {
                s_mouseReleased = false;
                captureMouse(true);
                continue;
            }
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_F10) {
                if (!ev.key.repeat) {
                    s_mouseReleased = s_mouseCaptured;
                    captureMouse(!s_mouseCaptured);
                }
                continue;
            }
        }
        // F11 or Alt+Enter toggles fullscreen and is kept from the pad layer
        if ((ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) &&
            (ev.key.keysym.scancode == SDL_SCANCODE_F11 ||
             ((ev.key.keysym.scancode == SDL_SCANCODE_RETURN || ev.key.keysym.scancode == SDL_SCANCODE_KP_ENTER) &&
              (ev.key.keysym.mod & KMOD_ALT)))) {
            if (ev.type == SDL_KEYDOWN && !ev.key.repeat) setFullscreen(!s_isFullscreen);
            continue;
        }
        // F7 with the overlay open cycles the game speed
        if (ev.type == SDL_KEYDOWN && ev.key.keysym.scancode == SDL_SCANCODE_F7 && GXPC_OverlayVisible()) {
            if (!ev.key.repeat) GXPC_CycleSpeed();
            continue;
        }
        if (s_eventCb) s_eventCb(&ev);
    }
#endif
}

}  // extern "C"
