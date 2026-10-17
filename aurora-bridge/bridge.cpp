// sms_aurora.dll: see sms_aurora.h.
#define SMS_AURORA_BUILD
#include "sms_aurora.h"
#include "depth_z24x8.hpp"
#include "sms_aurora_ui.h"
#include "ui_internal.h"
#include "gfx/common.hpp"
#include "gfx/render_worker.hpp"
#include "gfx/pipeline_cache.hpp"
#include "gfx/sms_readback.hpp"
#include "gx/fifo.hpp"
#include "webgpu/gpu.hpp"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <aurora/sms_ext.h>
#include <dolphin/gx.h>
#include <dolphin/gx/GXExtra.h>
#include <dolphin/vi.h>

#include <SDL3/SDL.h>

#include <csignal>
#include <cstdio>
#include <cstring>
#include <exception>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <chrono>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace {
// Aurora's command processor turns the game's GX stream into draws. It used
// to run inside sab_process, on the game thread: about a fifth of a busy
// frame (Pinna Park) went there while the game waited. This thread runs it
// instead, in order, while the game builds the rest of the frame. Every other
// entry point waits until the thread has caught up (sab_gx_sync), so Aurora's
// state is never used from two threads at once. SMS_AURORA_GX_THREAD=0 keeps
// the processing on the game thread.
class GxThread {
public:
    void start() {
        const char* v = std::getenv("SMS_AURORA_GX_THREAD");
        if ((v && std::strcmp(v, "0") == 0) || m_thread.joinable()) return;
        m_stop = false;
        m_thread = std::thread([this] { run(); });
    }
    void finish() {
        if (!m_thread.joinable()) return;
        {
            std::lock_guard lock(m_mutex);
            m_stop = true;
        }
        m_work.notify_all();
        m_thread.join();
    }
    bool running() const { return m_thread.joinable(); }
    void push(const void* data, uint32_t size) {
        std::vector<uint8_t> buffer;
        {
            std::unique_lock lock(m_mutex);
            // A few chunks ahead is enough to overlap; more only adds latency.
            m_space.wait(lock, [this] { return m_queue.size() < kMaxQueued; });
            if (!m_free.empty()) {
                buffer = std::move(m_free.back());
                m_free.pop_back();
            }
        }
        buffer.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
        {
            std::lock_guard lock(m_mutex);
            m_queue.push_back(std::move(buffer));
        }
        m_work.notify_one();
    }
    void sync() {
        if (!m_thread.joinable() || std::this_thread::get_id() == m_thread.get_id()) return;
        std::unique_lock lock(m_mutex);
        m_idle.wait(lock, [this] { return m_queue.empty() && !m_busy; });
    }

private:
    static constexpr size_t kMaxQueued = 3;
    void run() {
        for (;;) {
            std::vector<uint8_t> buffer;
            {
                std::unique_lock lock(m_mutex);
                m_work.wait(lock, [this] { return m_stop || !m_queue.empty(); });
                if (m_queue.empty()) return;
                buffer = std::move(m_queue.front());
                m_queue.pop_front();
                m_busy = true;
            }
            m_space.notify_one();
            GXCallDisplayListLE(buffer.data(), uint32_t(buffer.size()));
            {
                std::lock_guard lock(m_mutex);
                m_busy = false;
                if (m_free.size() < kMaxQueued + 1) m_free.push_back(std::move(buffer));
                if (m_queue.empty()) m_idle.notify_all();
            }
        }
    }
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_work, m_space, m_idle;
    std::deque<std::vector<uint8_t>> m_queue;
    std::vector<std::vector<uint8_t>> m_free;
    bool m_busy = false;
    bool m_stop = false;
};
GxThread s_gxThread;

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

void printContextStack(CONTEXT* ctx) {
    HANDLE proc = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
    SymInitialize(proc, nullptr, TRUE);
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = ctx->Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx->Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx->Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    CONTEXT ctxCopy = *ctx;
    alignas(SYMBOL_INFO) char buf[sizeof(SYMBOL_INFO) + 256];
    for (int i = 0; i < 48; i++) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, proc, thread, &frame, &ctxCopy, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
        DWORD64 addr = frame.AddrPC.Offset;
        if (!addr) break;
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(buf);
        std::memset(buf, 0, sizeof buf);
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 255;
        DWORD64 disp = 0;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof line;
        DWORD ldisp = 0;
        char mod[MAX_PATH] = "?";
        HMODULE hm = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(addr), &hm))
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

// Include faults in Dawn too: a render-resource lifetime failure can surface
// in the graphics DLL after leaving this bridge's stack frame.
LONG CALLBACK onException(EXCEPTION_POINTERS* info) {
    static bool printed = false;
    if (printed || info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    HMODULE self = nullptr, at = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&onException), &self);
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(info->ExceptionRecord->ExceptionAddress), &at);
    char module[MAX_PATH]{"?"};
    if (at && GetModuleFileNameA(at,module,MAX_PATH)) {}
    const char* base=std::strrchr(module,'\\');
    printed = true;
    std::fprintf(stderr, "[sms_aurora] access violation at %s+0x%llx (%p) reading/writing %p\n",
                 base?base+1:module,
                 at ? (unsigned long long)(reinterpret_cast<DWORD64>(info->ExceptionRecord->ExceptionAddress) - reinterpret_cast<DWORD64>(at)) : 0ULL,
                 info->ExceptionRecord->ExceptionAddress,
                 reinterpret_cast<void*>(info->ExceptionRecord->ExceptionInformation[1]));
    if (info->ContextRecord) printContextStack(info->ContextRecord);
    else printStack("access violation");
    return EXCEPTION_CONTINUE_SEARCH;
}
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
    if (sab_ui_process_sdl(e)) return;
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

void sab_gx_sync() { s_gxThread.sync(); }

extern "C" {

int sab_init(const SabConfig* c) {
    if (s_ready) return 1;
    std::signal(SIGABRT, onAbort);
    AddVectoredExceptionHandler(1, onException);
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
    sab_configure_present();
    AuroraInfo info = aurora_initialize(1, argv, &cfg);
    s_backend = info.backend;
    s_fifo.resize(256 * 1024);
    GXInit(s_fifo.data(), uint32_t(s_fifo.size()));
    s_ready = true;
    s_gxThread.start();
    pumpAurora();  // opens gamepads: Aurora initialises input on the first update
    const char* precompile = std::getenv("SMS_AURORA_PRECOMPILE");
    if (!precompile || std::strcmp(precompile, "0")) {
        SDL_Window* window = mainWindow();
        const std::string title = window ? SDL_GetWindowTitle(window) : cfg.appName;
        int lastPercent = -1;
        const bool prepared = aurora::gfx::precompile_pipeline_cache([&](size_t ready, size_t total) {
            pumpAurora();
            for (const auto& event : s_pending) if (event.type == SAB_EV_QUIT) return false;
            const int percent = total ? int((std::min)(ready, total) * 100 / total) : 100;
            if (window && percent != lastPercent) {
                const auto caption = title + " - Preparation des shaders : " + std::to_string(percent) + "%";
                SDL_SetWindowTitle(window, caption.c_str());
                lastPercent = percent;
            }
            return true;
        });
        if (window) SDL_SetWindowTitle(window, title.c_str());
        if (!prepared) {
            s_gxThread.finish();
            s_ready = false;
            aurora_shutdown();
            return 0;
        }
    }
    return 1;
}

void sab_shutdown(void) {
    if (!s_ready) return;
    s_gxThread.sync();
    s_gxThread.finish();
    sab_ui_shutdown();
    s_ready = false;
    aurora_shutdown();
}

const char* sab_backend_name(void) {
    sab_gx_sync();
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
    sab_gx_sync();
    if (!s_ready) return 0;
    if (s_pending.empty()) {
        pumpAurora();
        if (s_pending.empty()) return 0;
    }
    *out = s_pending.front();
    s_pending.pop_front();
    return 1;
}

int sab_begin_data_pass(uint16_t width, uint16_t height) {
    sab_gx_sync();
    if (!s_ready || !width || !height || width > 1024 || height > 1024) return 0;
    aurora::gx::fifo::drain();
    return aurora::gfx::begin_data_pass(width, height) ? 1 : 0;
}

uint32_t sab_copy_readback_capacity(void) { sab_gx_sync(); return aurora::gfx::sms_readback::copy_capacity(); }

int sab_flush_copy_readbacks(void) {
    sab_gx_sync();
    if (!s_ready || aurora::gfx::is_offscreen()) return 0;
    aurora::gx::fifo::drain();
    // A recording-thread checkpoint can safely rotate the mapped staging
    // buffer and submit its sealed prefix before waiting for occupied slots.
    if (!aurora::gfx::submit_copy_checkpoint()) return 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
        if (aurora::gfx::sms_readback::wait_for_copy_capacity(0)) return 1;
        // Use the same recording-thread/idle-worker contract as Aurora's
        // staging waits. A sleeping CV alone does not advance Dawn events.
        if (aurora::gfx::render_worker::is_idle()) aurora::webgpu::g_instance.ProcessEvents();
        if (aurora::gfx::sms_readback::wait_for_copy_capacity(1)) return 1;
    } while (std::chrono::steady_clock::now() < deadline);
    return 0;
}

void sab_end_data_pass(void) {
    sab_gx_sync();
    if (!s_ready) return;
    aurora::gx::fifo::drain();
    aurora::gfx::end_data_pass();
}

void sab_set_efb_size(uint32_t width, uint32_t height) {
    sab_gx_sync();
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
    sab_gx_sync();
    int ww = 0, hh = 0;
    if (SDL_Window* w = mainWindow()) SDL_GetWindowSizeInPixels(w, &ww, &hh);
    if (width) *width = uint32_t(ww);
    if (height) *height = uint32_t(hh);
}

void sab_set_fullscreen(int on) { sab_gx_sync(); VISetWindowFullscreen(on != 0); }
int sab_is_fullscreen(void) { sab_gx_sync(); return VIGetWindowFullscreen() ? 1 : 0; }
void sab_set_vsync(int on) { sab_gx_sync(); aurora_enable_vsync(on != 0); }
void sab_set_window_title(const char* title) { sab_gx_sync(); VISetWindowTitle(title); }

void sab_set_mouse_relative(int on) {
    sab_gx_sync();
    if (SDL_Window* w = mainWindow()) SDL_SetWindowRelativeMouseMode(w, on != 0);
}

int sab_begin_frame(void) {
    sab_gx_sync();
    if (!s_ready) return 0;
    if (aurora::webgpu::g_instance && aurora::gfx::render_worker::is_idle()) {
        aurora::webgpu::g_instance.ProcessEvents();
    }
    sab_apply_pending_settings();
    return aurora_begin_frame() ? 1 : 0;
}
void sab_end_frame(void) {
    sab_gx_sync();
    if (s_ready) { sab_ui_overlay_draw();aurora_end_frame(); }
}

void sab_finish_frame(int present) {
    sab_gx_sync();
    if (!s_ready) return;
    if (present) sab_ui_overlay_draw();
    aurora::finish_recording_frame(present!=0);
}
void sab_retain_efb(void) { sab_gx_sync(); if (s_ready) aurora::gfx::retain_efb_for_ui(); }
int sab_copy_display(const void* dest,uint16_t x,uint16_t y,uint16_t w,uint16_t h,int clear) {
    sab_gx_sync();
    if (!s_ready) return 0;
    aurora::gx::fifo::drain();
    return aurora::gfx::copy_display(dest,{x,y,w,h},clear!=0)?1:0;
}
int sab_select_scanout(const void* dest,int black) {
    sab_gx_sync();
    return s_ready && aurora::gfx::select_scanout(dest,black!=0)?1:0;
}

void sab_process(const void* data, uint32_t size) {
    if (!s_ready || !data || !size) return;
    if (s_gxThread.running()) s_gxThread.push(data, size);
    else GXCallDisplayListLE(data, size);
}

int sab_peek_efb(uint32_t x, uint32_t y, uint64_t revision, uint32_t* argb, uint32_t* z) {
    sab_gx_sync();
    if (!s_ready || !argb || !z) return 0;
    aurora::gx::fifo::drain();
    if (!aurora::gfx::peek_efb(x,y,revision,*argb,*z)) return 0;
    return 1;
}

int sab_copy_depth_z24x8_sync(void* dest, uint16_t left, uint16_t top, uint16_t width, uint16_t height,
                             uint16_t dstWidth, uint16_t dstHeight) {
    sab_gx_sync();
    if (!s_ready || !dest || !width || !height || !dstWidth || !dstHeight ||
        width > 1024 || height > 1024 || dstWidth > 1024 || dstHeight > 1024) return 0;
    aurora::gx::fifo::drain();
    std::vector<uint32_t> depth;
    if (!aurora::gfx::copy_depth_efb({left, top, width, height}, depth) ||
        depth.size() != size_t(width) * height) return 0;
    std::vector<uint8_t> encoded;
    if (!sms_aurora_depth::encode_z24x8(depth.data(), width, height, dstWidth, dstHeight, encoded)) return 0;
    std::memcpy(dest, encoded.data(), encoded.size());
    return 1;
}

int sab_copy_depth_z24x8_async(uint16_t left, uint16_t top, uint16_t width, uint16_t height, uint64_t tag) {
    sab_gx_sync();
    if (!s_ready || !tag || !width || !height || width > 1024 || height > 1024) return 0;
    aurora::gx::fifo::drain();
    return aurora::gfx::request_depth_efb_async({left, top, width, height}, tag) ? 1 : 0;
}

int sab_take_depth_z24x8(uint64_t tag, void* dest, uint16_t width, uint16_t height, uint16_t dstWidth,
                         uint16_t dstHeight) {
    sab_gx_sync();
    if (!s_ready) return 0;
    if (aurora::webgpu::g_instance && aurora::gfx::render_worker::is_idle()) {
        aurora::webgpu::g_instance.ProcessEvents();
    }
    static std::vector<uint32_t> depth;
    uint64_t doneTag = 0;
    bool success = false;
    if (!aurora::gfx::take_depth_efb_async(doneTag, depth, success)) return 0;
    if (doneTag != tag || !success || !dest || depth.size() != size_t(width) * height) return -1;
    std::vector<uint8_t> encoded;
    if (!sms_aurora_depth::encode_z24x8(depth.data(), width, height, dstWidth, dstHeight, encoded)) return -1;
    std::memcpy(dest, encoded.data(), encoded.size());
    return 1;
}

void sab_copy_tex(void* dest, uint16_t left, uint16_t top, uint16_t width, uint16_t height, uint16_t dstWidth,
                  uint16_t dstHeight, uint32_t fmt, int clear, int readback) {
    sab_gx_sync();
    sab_copy_tex_request(dest, left, top, width, height, dstWidth, dstHeight, fmt, clear, readback, 0);
}

void sab_copy_tex_request(void* dest, uint16_t left, uint16_t top, uint16_t width, uint16_t height,
                          uint16_t dstWidth, uint16_t dstHeight, uint32_t fmt, int clear, int readback,
                          uint64_t requestId) {
    sab_gx_sync();
    if (!s_ready) return;
    if (readback) {
        // GX depth-copy formats carry the Z-format bit; this colour readback
        // shader cannot return depth data.
        if (!dest || !dstWidth || !dstHeight || (fmt & 0x10u))
            aurora_sms_reject_copy_readback(dest, dstWidth, dstHeight, requestId);
        else
            aurora_sms_request_copy_readback(dest, dstWidth, dstHeight, requestId);
    }
    GXSetTexCopySrc(left, top, width, height);
    GXSetTexCopyDst(dstWidth, dstHeight, GXTexFmt(fmt), GX_FALSE);
    GXCopyTex(dest, clear ? GX_TRUE : GX_FALSE);
}

int sab_poll_copy_readback(const void** dest, uint32_t* width, uint32_t* height, uint64_t* frameId,
                           const uint32_t** rgba) {
    sab_gx_sync();
    uint64_t requestId;
    int success;
    // Older callers cannot consume failure notifications or request IDs.
    while (sab_poll_copy_readback_request(dest, width, height, frameId, rgba, &requestId, &success)) {
        if (success) return 1;
    }
    return 0;
}

int sab_poll_copy_readback_request(const void** dest, uint32_t* width, uint32_t* height, uint64_t* frameId,
                                   const uint32_t** rgba, uint64_t* requestId, int* success) {
    sab_gx_sync();
    if (!s_ready) return 0;
    if (aurora::webgpu::g_instance && aurora::gfx::render_worker::is_idle()) {
        aurora::webgpu::g_instance.ProcessEvents();
    }
    return aurora_sms_poll_copy_readback(dest, width, height, frameId, rgba, requestId, success);
}

uint64_t sab_frame_id(void) { sab_gx_sync(); return aurora_sms_current_frame_id(); }
float sab_efb_pixel_area_scale(void) { sab_gx_sync(); return aurora_sms_efb_pixel_area_scale(); }
int sab_poll_pixel_metrics(uint64_t* frameId, const uint32_t** counts) {
    sab_gx_sync();
    return s_ready ? aurora_sms_poll_pixel_metrics(frameId, counts) : 0;
}

void sab_destroy_copy_tex(void* dest) {
    sab_gx_sync();
    if (s_ready) GXDestroyCopyTex(dest);
}

void sab_set_window_icon(const uint8_t* rgba, int width, int height) {
    sab_gx_sync();
    SDL_Window* w = mainWindow();
    if (!w || !rgba || width <= 0 || height <= 0) return;
    if (SDL_Surface* s =
            SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, const_cast<uint8_t*>(rgba), width * 4)) {
        SDL_SetWindowIcon(w, s);
        SDL_DestroySurface(s);
    }
}

}  // extern "C"
