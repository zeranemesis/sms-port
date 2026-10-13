// sms_aurora.dll: see sms_aurora.h.
#define SMS_AURORA_BUILD
#include "sms_aurora.h"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <dolphin/gx.h>
#include <dolphin/gx/GXExtra.h>
#include <dolphin/vi.h>

#include <SDL3/SDL.h>

#include <csignal>
#include <cstdio>
#include <cstring>
#include <exception>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <deque>
#include <vector>

namespace {
bool s_ready = false;
AuroraBackend s_backend = BACKEND_AUTO;
std::deque<SabEvent> s_pending;
std::vector<uint8_t> s_fifo;

void logCallback(AuroraLogLevel level, const char* module, const char* message, unsigned int len) {
    static const char* const kLevel[] = {"debug", "info", "warning", "error", "fatal"};
    std::fprintf(stderr, "[aurora %s] %s: %.*s\n", kLevel[level <= LOG_FATAL ? level : LOG_ERROR], module, int(len),
                 message);
    if (level >= LOG_ERROR) std::fflush(stderr);
}

SDL_Window* mainWindow() {
    int count = 0;
    SDL_Window** list = SDL_GetWindows(&count);
    SDL_Window* w = list && count > 0 ? list[0] : nullptr;
    SDL_free(list);
    return w;
}

// abort() inside Aurora or Dawn ends the process without a word: print where.
void printStack(const char* why) {
    std::fprintf(stderr, "[sms_aurora] %s, stack:\n", why);
    HANDLE proc = GetCurrentProcess();
    static bool symInit = false;
    if (!symInit) {
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        SymInitialize(proc, nullptr, TRUE);
        symInit = true;
    }
    void* frames[48];
    USHORT n = CaptureStackBackTrace(1, 48, frames, nullptr);
    alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 256];
    for (USHORT i = 0; i < n; i++) {
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
        std::memset(buf, 0, sizeof buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof line;
        DWORD ldisp = 0;
        const DWORD64 addr = reinterpret_cast<DWORD64>(frames[i]);
        char mod[MAX_PATH] = "?";
        HMODULE hm = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(frames[i]), &hm))
            GetModuleFileNameA(hm, mod, sizeof mod);
        const char* base = std::strrchr(mod, '\\');
        if (SymFromAddr(proc, addr, &disp, sym)) {
            if (SymGetLineFromAddr64(proc, addr, &ldisp, &line))
                std::fprintf(stderr, "  %s!%s+0x%llx (%s:%lu)\n", base ? base + 1 : mod, sym->Name,
                             (unsigned long long)disp, line.FileName, line.LineNumber);
            else
                std::fprintf(stderr, "  %s!%s+0x%llx\n", base ? base + 1 : mod, sym->Name, (unsigned long long)disp);
        } else {
            std::fprintf(stderr, "  %s+0x%llx\n", base ? base + 1 : mod,
                         (unsigned long long)(addr - reinterpret_cast<DWORD64>(hm)));
        }
    }
    std::fflush(stderr);
}

void onAbort(int) { printStack("abort"); }
void onTerminate() {
    const char* what = "terminate";
    try {
        if (auto e = std::current_exception()) std::rethrow_exception(e);
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[sms_aurora] uncaught exception: %s\n", ex.what());
    } catch (...) {
    }
    printStack(what);
    std::_Exit(3);
}

void push(const SabEvent& ev) { s_pending.push_back(ev); }

void translate(const SDL_Event& e) {
    SabEvent ev{};
    switch (e.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        ev.type = SAB_EV_QUIT;
        push(ev);
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        ev.type = e.type == SDL_EVENT_KEY_DOWN ? SAB_EV_KEYDOWN : SAB_EV_KEYUP;
        ev.scancode = int32_t(e.key.scancode);
        ev.mod = e.key.mod;
        ev.repeat = e.key.repeat ? 1 : 0;
        push(ev);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        ev.type = SAB_EV_MOUSEMOTION;
        ev.xrel = e.motion.xrel;
        ev.yrel = e.motion.yrel;
        push(ev);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        ev.type = SAB_EV_MOUSEBUTTONDOWN;
        ev.button = e.button.button;
        push(ev);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        ev.type = SAB_EV_PADAXIS;
        ev.which = int32_t(e.gaxis.which);
        ev.button = e.gaxis.axis;
        ev.value = e.gaxis.value;
        push(ev);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        ev.type = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? SAB_EV_PADBUTTONDOWN : SAB_EV_PADBUTTONUP;
        ev.which = int32_t(e.gbutton.which);
        ev.button = e.gbutton.button;
        push(ev);
        break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        ev.type = SAB_EV_FOCUS_GAINED;
        push(ev);
        break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        ev.type = SAB_EV_FOCUS_LOST;
        push(ev);
        break;
    default:
        break;
    }
}

void pumpAurora() {
    for (const AuroraEvent* e = aurora_update(); e && e->type != AURORA_NONE; ++e) {
        if (e->type == AURORA_EXIT) {
            SabEvent ev{};
            ev.type = SAB_EV_QUIT;
            push(ev);
        } else if (e->type == AURORA_SDL_EVENT) {
            translate(e->sdl);
        }
    }
}
}  // namespace

extern "C" {

int sab_init(const SabConfig* c) {
    if (s_ready) return 1;
    std::signal(SIGABRT, onAbort);
    std::set_terminate(onTerminate);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    AuroraConfig cfg{};
    cfg.appName = c && c->title ? c->title : "Super Mario Sunshine";
    cfg.userPath = c ? c->userPath : nullptr;
    cfg.desiredBackend = c ? AuroraBackend(c->backend) : BACKEND_AUTO;
    cfg.msaa = c && c->msaa ? c->msaa : 1;
    cfg.maxTextureAnisotropy = uint16_t(c && c->anisotropy ? c->anisotropy : 16);
    cfg.vsync = c ? c->vsync != 0 : true;
    cfg.startFullscreen = c && c->fullscreen;
    cfg.allowJoystickBackgroundEvents = true;
    cfg.windowPosX = -1;
    cfg.windowPosY = -1;
    cfg.windowWidth = c && c->windowWidth ? c->windowWidth : 1280;
    cfg.windowHeight = c && c->windowHeight ? c->windowHeight : 960;
    cfg.logCallback = logCallback;
    cfg.logLevel = c ? AuroraLogLevel(c->logLevel) : LOG_INFO;
    cfg.mem1Size = 0;  // the game has its own MEM1 (platform/os)
    cfg.mem2Size = 0;
    char arg0[] = "sms";
    char* argv[] = {arg0, nullptr};
    AuroraInfo info = aurora_initialize(1, argv, &cfg);
    s_backend = info.backend;
    s_fifo.resize(256 * 1024);
    GXInit(s_fifo.data(), uint32_t(s_fifo.size()));
    s_ready = true;
    pumpAurora();  // opens gamepads: Aurora initialises input on the first update
    return 1;
}

void sab_shutdown(void) {
    if (!s_ready) return;
    s_ready = false;
    aurora_shutdown();
}

const char* sab_backend_name(void) {
    switch (s_backend) {
    case BACKEND_D3D11: return "Direct3D 11";
    case BACKEND_D3D12: return "Direct3D 12";
    case BACKEND_VULKAN: return "Vulkan";
    case BACKEND_OPENGL: return "OpenGL";
    case BACKEND_OPENGLES: return "OpenGL ES";
    case BACKEND_METAL: return "Metal";
    case BACKEND_NULL: return "null";
    default: return "auto";
    }
}

int sab_poll_event(SabEvent* out) {
    if (!s_ready) return 0;
    if (s_pending.empty()) {
        pumpAurora();
        if (s_pending.empty()) return 0;
    }
    *out = s_pending.front();
    s_pending.pop_front();
    return 1;
}

void sab_set_efb_size(uint32_t width, uint32_t height) {
    static GXRenderModeObj rm;
    std::memset(&rm, 0, sizeof rm);
    rm.fbWidth = uint16_t(width);
    rm.efbHeight = uint16_t(height);
    rm.xfbHeight = uint16_t(height);
    rm.viWidth = uint16_t(width);
    rm.viHeight = uint16_t(height);
    VIConfigure(&rm);
}

void sab_get_window_size(uint32_t* width, uint32_t* height) {
    int ww = 0, hh = 0;
    if (SDL_Window* w = mainWindow()) SDL_GetWindowSizeInPixels(w, &ww, &hh);
    if (width) *width = uint32_t(ww);
    if (height) *height = uint32_t(hh);
}

void sab_set_fullscreen(int on) { VISetWindowFullscreen(on != 0); }
int sab_is_fullscreen(void) { return VIGetWindowFullscreen() ? 1 : 0; }
void sab_set_vsync(int on) { (void)on; }
void sab_set_window_title(const char* title) { VISetWindowTitle(title); }

void sab_set_mouse_relative(int on) {
    if (SDL_Window* w = mainWindow()) SDL_SetWindowRelativeMouseMode(w, on != 0);
}

int sab_begin_frame(void) { return s_ready && aurora_begin_frame() ? 1 : 0; }
void sab_end_frame(void) {
    if (s_ready) aurora_end_frame();
}

void sab_process(const void* data, uint32_t size) {
    if (s_ready && data && size) GXCallDisplayListLE(data, size);
}

void sab_copy_tex(void* dest, uint16_t left, uint16_t top, uint16_t width, uint16_t height, uint16_t dstWidth,
                  uint16_t dstHeight, uint32_t fmt, int clear) {
    if (!s_ready) return;
    GXSetTexCopySrc(left, top, width, height);
    GXSetTexCopyDst(dstWidth, dstHeight, GXTexFmt(fmt), GX_FALSE);
    GXCopyTex(dest, clear ? GX_TRUE : GX_FALSE);
}

void sab_destroy_copy_tex(void* dest) {
    if (s_ready) GXDestroyCopyTex(dest);
}

void sab_set_window_icon(const uint8_t* rgba, int width, int height) {
    SDL_Window* w = mainWindow();
    if (!w || !rgba || width <= 0 || height <= 0) return;
    if (SDL_Surface* s =
            SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, const_cast<uint8_t*>(rgba), width * 4)) {
        SDL_SetWindowIcon(w, s);
        SDL_DestroySurface(s);
    }
}

}  // extern "C"
