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
#include <aurora/imgui.h>
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
#include <chrono>
#include <functional>
#include <deque>
#include <memory>
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
    // Nothing queued or running: Aurora's state can be used without waiting.
    bool idle() {
        if (!m_thread.joinable()) return true;
        std::lock_guard lock(m_mutex);
        return m_queue.empty() && !m_busy;
    }
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
            m_queue.push_back(Item{std::move(buffer), {}});
        }
        m_work.notify_one();
    }
    // Work that uses Aurora's state but returns nothing to the game (a texture
    // copy, a data pass), run in order with the stream.
    void pushJob(std::function<void()> job) {
        {
            std::unique_lock lock(m_mutex);
            m_space.wait(lock, [this] { return m_queue.size() < kMaxQueued; });
            m_queue.push_back(Item{{}, std::move(job)});
        }
        m_work.notify_one();
    }
    void sync() {
        if (!m_thread.joinable() || std::this_thread::get_id() == m_thread.get_id()) return;
        const auto t0 = std::chrono::steady_clock::now();
        std::unique_lock lock(m_mutex);
        m_idle.wait(lock, [this] { return m_queue.empty() && !m_busy; });
        m_waited += std::chrono::steady_clock::now() - t0;
    }
    // Statistics: time the thread spent working, and callers spent waiting for it.
    void takeTimes(double* busy, double* waited) {
        std::lock_guard lock(m_mutex);
        *busy = std::chrono::duration<double>(m_busyTime).count();
        *waited = std::chrono::duration<double>(m_waited).count();
        m_busyTime = m_waited = {};
    }

private:
    // Copies and data passes queue too: room for a frame's worth of them.
    static constexpr size_t kMaxQueued = 16;
    void run() {
        for (;;) {
            Item item;
            {
                std::unique_lock lock(m_mutex);
                m_work.wait(lock, [this] { return m_stop || !m_queue.empty(); });
                if (m_queue.empty()) return;
                item = std::move(m_queue.front());
                m_queue.pop_front();
                m_busy = true;
            }
            m_space.notify_one();
            const auto t0 = std::chrono::steady_clock::now();
            if (item.job) item.job();
            else GXCallDisplayListLE(item.bytes.data(), uint32_t(item.bytes.size()));
            const auto spent = std::chrono::steady_clock::now() - t0;
            {
                std::lock_guard lock(m_mutex);
                m_busyTime += spent;
                m_busy = false;
                if (!item.job && m_free.size() < kMaxQueued + 1) m_free.push_back(std::move(item.bytes));
                if (m_queue.empty()) m_idle.notify_all();
            }
        }
    }
    std::thread m_thread;
    std::mutex m_mutex;
    std::condition_variable m_work, m_space, m_idle;
    struct Item {
        std::vector<uint8_t> bytes;
        std::function<void()> job;
    };
    std::deque<Item> m_queue;
    std::vector<std::vector<uint8_t>> m_free;
    bool m_busy = false;
    bool m_stop = false;
    std::chrono::steady_clock::duration m_busyTime{}, m_waited{};
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
    case SDL_EVENT_MOUSE_BUTTON_UP:
        ev.type = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? SAB_EV_MOUSEBUTTONDOWN : SAB_EV_MOUSEBUTTONUP;
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

// The shader preparation, drawn in the window: on a first launch it lasts minutes,
// and a black window looks like a game that does not start.
void drawPreparation(int percent) {
    if (!aurora_begin_frame()) return;
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(size.x * .5f, size.y * .5f), ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(ImVec2(size.x * .6f, 0.f), ImGuiCond_Always);
    if (ImGui::Begin("##shader-preparation", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav)) {
        ImGui::TextUnformatted("Preparation des shaders : quelques minutes au premier lancement");
        ImGui::TextUnformatted("Preparing shaders: a few minutes on the first launch");
        ImGui::ProgressBar(float(percent) / 100.f, ImVec2(-1.f, 0.f));
    }
    ImGui::End();
    aurora_end_frame();
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
    // Frame generation (SMS_FRAMEGEN, gx_aurora.cpp) keeps a third frame in flight:
    // its generated image waits for its present while the next frame records.
    if (const char* fg = std::getenv("SMS_FRAMEGEN"); fg && *fg && std::strcmp(fg, "0") != 0)
        aurora_sms_set_frames_in_flight(3);
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
        size_t checkedEvents = 0;
        auto lastDrawn = std::chrono::steady_clock::now() - std::chrono::seconds(1);
        const bool prepared = aurora::gfx::precompile_pipeline_cache([&](size_t ready, size_t total) {
            // The preparation UI can still have a frame on Aurora's render worker.
            // Drain both workers before SDL applies resize/fullscreen events, just
            // as the game must do before changing the window between frames.
            sab_gx_sync();
            if (!aurora::gfx::render_worker::is_idle()) aurora::gfx::render_worker::synchronize();
            pumpAurora();
            // The game has not started polling yet. Keep its other input in order,
            // inspecting only events added since the previous callback. Consumed
            // window shortcuts must not toggle again when game polling begins.
            size_t remaining = s_pending.size() - checkedEvents;
            while (remaining-- > 0) {
                const auto& event = s_pending[checkedEvents];
                if (event.type == SAB_EV_QUIT) return false;
                const bool windowShortcut = (event.type == SAB_EV_KEYDOWN || event.type == SAB_EV_KEYUP) &&
                    (event.scancode == SDL_SCANCODE_F11 ||
                     ((event.scancode == SDL_SCANCODE_RETURN || event.scancode == SDL_SCANCODE_KP_ENTER) &&
                      (event.mod & SDL_KMOD_ALT)));
                if (windowShortcut) {
                    if (event.type == SAB_EV_KEYDOWN && !event.repeat)
                        VISetWindowFullscreen(!VIGetWindowFullscreen());
                    s_pending.erase(s_pending.begin() + checkedEvents);
                } else {
                    ++checkedEvents;
                }
            }
            const int percent = total ? int((std::min)(ready, total) * 100 / total) : 100;
            if (window && percent != lastPercent) {
                const auto caption = title + " - Preparation des shaders : " + std::to_string(percent) + "%";
                SDL_SetWindowTitle(window, caption.c_str());
                lastPercent = percent;
            }
            if (std::chrono::steady_clock::now() - lastDrawn > std::chrono::milliseconds(100)) {
                drawPreparation(percent);
                lastDrawn = std::chrono::steady_clock::now();
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
    if (!s_ready) return 0;
    // Pumping SDL's events touches Aurora's window: only while the GX thread
    // is idle. Events already taken are handed out either way.
    if (s_pending.empty() && s_gxThread.idle()) pumpAurora();
    if (s_pending.empty()) return 0;
    *out = s_pending.front();
    s_pending.pop_front();
    return 1;
}

// With the GX thread, a data pass begins and ends in order on it, without the
// game waiting: whether it began is kept there (s_dataPassOpen), so a pass that
// could not begin is not ended.
bool s_dataPassOpen = false;  // GX thread
int sab_begin_data_pass(uint16_t width, uint16_t height) {
    if (!s_ready || !width || !height || width > 1024 || height > 1024) return 0;
    if (s_gxThread.running()) {
        s_gxThread.pushJob([width, height] {
            aurora::gx::fifo::drain();
            s_dataPassOpen = aurora::gfx::begin_data_pass(width, height);
        });
        return 1;
    }
    aurora::gx::fifo::drain();
    return aurora::gfx::begin_data_pass(width, height) ? 1 : 0;
}

// A constant: no need to wait for the GX thread.
uint32_t sab_copy_readback_capacity(void) { return aurora::gfx::sms_readback::copy_capacity(); }

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
    if (!s_ready) return;
    if (s_gxThread.running()) {
        s_gxThread.pushJob([] {
            if (!s_dataPassOpen) return;
            s_dataPassOpen = false;
            aurora::gx::fifo::drain();
            aurora::gfx::end_data_pass();
        });
        return;
    }
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
void sab_present_after(uint32_t microseconds) { sab_gx_sync(); aurora_sms_present_after(microseconds); }

namespace {
// sab_begin_frame without the wait, on whichever thread runs it.
bool beginFrameNow() {
    if (!s_ready) return false;
    if (aurora::webgpu::g_instance && aurora::gfx::render_worker::is_idle()) {
        aurora::webgpu::g_instance.ProcessEvents();
    }
    sab_apply_pending_settings();
    return aurora_begin_frame();
}

// A frame made of recorded operations, run on the GX thread (or here when
// there is none): frame generation's replayed frame.
struct ReplayOp {
    int kind;
    const uint8_t* bytes;  // the caller's, valid until the job has run
    uint32_t size;
    void* dest;
    uint16_t x, y, w, h, dw, dh;
    uint32_t fmt;
    int clear;
};
// Statistics: where the GX thread's time goes (seconds): replay begin, stream,
// copies, end; asynchronous begin.
double s_gxSpent[5];
struct GxSpent {
    int slot;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    ~GxSpent() { s_gxSpent[slot] += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); }
};

void runReplay(const std::vector<ReplayOp>& ops, const void* xfb, std::chrono::steady_clock::time_point presentAt) {
    {
        GxSpent t{0};
        if (!beginFrameNow()) return;
    }
    for (const auto& op : ops) {
        GxSpent t{op.kind == SAB_REPLAY_STREAM ? 1 : 2};
        switch (op.kind) {
        case SAB_REPLAY_STREAM:
            GXCallDisplayListLE(op.bytes, op.size);
            break;
        case SAB_REPLAY_COPY_TEX:
            GXSetTexCopySrc(op.x, op.y, op.w, op.h);
            GXSetTexCopyDst(op.dw, op.dh, GXTexFmt(op.fmt), GX_FALSE);
            GXCopyTex(op.dest, op.clear ? GX_TRUE : GX_FALSE);
            break;
        case SAB_REPLAY_COPY_DISPLAY:
            aurora::gx::fifo::drain();
            aurora::gfx::copy_display(op.dest, {op.x, op.y, op.w, op.h}, op.clear != 0);
            break;
        case SAB_REPLAY_DATA_PASS_BEGIN:
            aurora::gx::fifo::drain();
            aurora::gfx::begin_data_pass(op.w, op.h);
            break;
        case SAB_REPLAY_DATA_PASS_END:
            aurora::gx::fifo::drain();
            aurora::gfx::end_data_pass();
            break;
        }
    }
    GxSpent t{3};
    aurora::gfx::select_scanout(xfb, false);
    const auto left = std::chrono::duration_cast<std::chrono::microseconds>(presentAt - std::chrono::steady_clock::now());
    aurora_sms_present_after(left.count() > 0 ? uint32_t(left.count()) : 0);
    sab_ui_overlay_draw();
    aurora::finish_recording_frame(true);
}
}  // namespace

void sab_submit_replay(const SabReplayOp* ops, uint32_t count, const void* xfb, uint32_t presentAfterUs) {
    if (!s_ready || !ops || !count) return;
    const auto presentAt = std::chrono::steady_clock::now() + std::chrono::microseconds(presentAfterUs);
    auto recorded = std::make_shared<std::vector<ReplayOp>>();
    recorded->reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        const SabReplayOp& o = ops[i];
        recorded->push_back(ReplayOp{o.kind, static_cast<const uint8_t*>(o.bytes), o.size, o.dest, o.x, o.y, o.w,
                                     o.h, o.dw, o.dh, o.fmt, o.clear});
    }
    if (s_gxThread.running()) {
        s_gxThread.pushJob([recorded, xfb, presentAt] { runReplay(*recorded, xfb, presentAt); });
    } else {
        runReplay(*recorded, xfb, presentAt);
    }
}

void sab_gx_thread_times(double* busySeconds, double* waitedSeconds) {
    *busySeconds = *waitedSeconds = 0;
    if (s_gxThread.running()) s_gxThread.takeTimes(busySeconds, waitedSeconds);
}

void sab_gx_thread_breakdown(double* seconds) {
    sab_gx_sync();
    for (int i = 0; i < 5; i++) {
        seconds[i] = s_gxSpent[i];
        s_gxSpent[i] = 0;
    }
}

void sab_begin_frame_async(int retainEfb) {
    if (!s_ready) return;
    auto job = [retainEfb] {
        GxSpent t{4};
        if (beginFrameNow() && retainEfb) aurora::gfx::retain_efb_for_ui();
    };
    if (s_gxThread.running()) s_gxThread.pushJob(job);
    else job();
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

namespace {
void copyTexNow(void* dest, uint16_t left, uint16_t top, uint16_t width, uint16_t height, uint16_t dstWidth,
                uint16_t dstHeight, uint32_t fmt, int clear, int readback, uint64_t requestId);
}

// Returns nothing to the caller: queued in order on the GX thread.
void sab_copy_tex_request(void* dest, uint16_t left, uint16_t top, uint16_t width, uint16_t height,
                          uint16_t dstWidth, uint16_t dstHeight, uint32_t fmt, int clear, int readback,
                          uint64_t requestId) {
    if (!s_ready) return;
    if (s_gxThread.running()) {
        s_gxThread.pushJob([=] {
            copyTexNow(dest, left, top, width, height, dstWidth, dstHeight, fmt, clear, readback, requestId);
        });
    } else {
        copyTexNow(dest, left, top, width, height, dstWidth, dstHeight, fmt, clear, readback, requestId);
    }
}

namespace {
void copyTexNow(void* dest, uint16_t left, uint16_t top, uint16_t width, uint16_t height, uint16_t dstWidth,
                uint16_t dstHeight, uint32_t fmt, int clear, int readback, uint64_t requestId) {
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
}  // namespace

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
    // A poll: while the GX thread is busy, the read-backs that are ready come
    // at the next one instead of the game waiting now.
    if (!s_gxThread.idle()) return 0;
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
