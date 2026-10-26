#define SMS_AURORA_BUILD
#include "sms_aurora_ui.h"
#include "sms_aurora_settings.h"
#include "ui_internal.h"
#include <aurora/rmlui.hpp>
#include <aurora/imgui.h>
#include <aurora/gfx.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/DecorationTypes.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi_Backend.h>
#include "webgpu/gpu.hpp"
#include "rmlui.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
SabUiCallbacks callbacks{};
Rml::ElementDocument* portalDocument = nullptr;
std::array<SDL_Cursor*, SDL_SYSTEM_CURSOR_COUNT> cursors{};
bool overlayVisible = false, effectDirty = true, effectFxaa = false;
float effectGamma = 1.f;
Rml::CompiledFilterHandle presentEffect = 0;

Rml::RenderInterface* renderer() { return Backend::GetRenderInterface(); }

// The native menu keeps its DOM and service state in MinGW. This empty
// element only places its C draw callback inside Aurora's WebGPU UI pass.
class Portal final : public Rml::Element {
public:
    explicit Portal(const Rml::String& tag) : Element(tag) {}
    void OnRender() override {
        if (effectDirty) {
            if (presentEffect) renderer()->ReleaseFilter(presentEffect);
            presentEffect = 0;
            if (effectFxaa || effectGamma != 1.f) {
                Rml::Dictionary parameters;
                parameters["gamma"] = effectGamma;
                parameters["fxaa"] = effectFxaa;
                presentEffect = renderer()->CompileFilter("sunshine-present", parameters);
            }
            effectDirty = false;
        }
        if (presentEffect) {
            renderer()->EnableScissorRegion(false);
            renderer()->EnableClipMask(false);
            renderer()->CompositeLayers(0, 0, Rml::BlendMode::Replace, {&presentEffect, 1});
        }
        auto* context = aurora::rmlui::get_context();
        if (callbacks.draw && context) {
            const auto dimensions = context->GetDimensions();
            callbacks.draw(callbacks.userdata, dimensions.x, dimensions.y,
                           context->GetDensityIndependentPixelRatio());
        }
    }
};
Rml::ElementInstancerGeneric<Portal> portalInstancer;

SDL_Window* window() {
    int count = 0;
    SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* result = count && windows ? windows[0] : nullptr;
    SDL_free(windows);
    return result;
}

void mousePosition(float x, float y, SabUiEvent& event) {
    int width = 0, height = 0, pixelWidth = 0, pixelHeight = 0;
    SDL_Window* w = window();
    if (w) {
        SDL_GetWindowSize(w, &width, &height);
        SDL_GetWindowSizeInPixels(w, &pixelWidth, &pixelHeight);
    }
    const auto& source = aurora::webgpu::present_source();
    const auto viewport = aurora::webgpu::calculate_present_viewport(
        uint32_t(std::max(pixelWidth, 1)), uint32_t(std::max(pixelHeight, 1)),
        std::max(source.size.width, 1u), std::max(source.size.height, 1u));
    const auto dimensions = aurora::rmlui::get_context()->GetDimensions();
#ifdef __ANDROID__
    // The mobile PartyBoard overlay fills the UI surface, including before
    // the game has submitted its first XFB. A stale 4:3 game source must not
    // displace touches on the full-screen menu or its bottom buttons.
    event.x = x * float(dimensions.x) / float(std::max(width, 1));
    event.y = y * float(dimensions.y) / float(std::max(height, 1));
    return;
#endif
    event.x = (x * float(pixelWidth) / float(std::max(width, 1)) - viewport.left) *
              float(dimensions.x) / std::max(viewport.width, 1.f);
    event.y = (y * float(pixelHeight) / float(std::max(height, 1)) - viewport.top) *
              float(dimensions.y) / std::max(viewport.height, 1.f);
}

Rml::Dictionary dictionary(const SabUiParameter* values, uint32_t count) {
    Rml::Dictionary result;
    for (uint32_t i = 0; i < count; ++i) {
        const auto& p = values[i];
        if (!p.name) continue;
        switch (p.type) {
        case SAB_UI_NUMBER: result[p.name] = p.value[0]; break;
        case SAB_UI_BOOLEAN: result[p.name] = p.value[0] != 0.f; break;
        case SAB_UI_VECTOR2: result[p.name] = Rml::Vector2f(p.value[0], p.value[1]); break;
        case SAB_UI_COLOR:
            result[p.name] = Rml::Colourb(uint8_t(p.value[0]), uint8_t(p.value[1]), uint8_t(p.value[2]), uint8_t(p.value[3]));
            break;
        case SAB_UI_STOPS: {
            Rml::ColorStopList stops;
            stops.reserve(p.stopCount);
            for (uint32_t j = 0; j < p.stopCount; ++j) {
                const auto& s = p.stops[j];
                stops.push_back({Rml::ColourbPremultiplied(s.color[0], s.color[1], s.color[2], s.color[3]),
                                 Rml::NumericValue(s.position, Rml::Unit(s.unit))});
            }
            result[p.name] = std::move(stops);
            break;
        }
        }
    }
    return result;
}
}

bool sab_ui_process_sdl(const SDL_Event& e) {
    sab_gx_sync();
    if (!callbacks.event || !portalDocument) return false;
    SabUiEvent out{};
    out.modifiers = uint16_t(SDL_GetModState());
#ifdef __ANDROID__
    // SDL's synthesized mouse supports taps, but cannot scroll panes by drag.
    // Delay the click until release so a swipe never activates its first row.
    static bool touchActive = false, touchScrolled = false;
    static SDL_FingerID touchFinger = 0;
    static float touchX = 0, touchY = 0, lastY = 0;
    const bool finger = e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_MOTION
        || e.type == SDL_EVENT_FINGER_UP || e.type == SDL_EVENT_FINGER_CANCELED;
    if (finger) {
        if (e.type != SDL_EVENT_FINGER_DOWN && (!touchActive || e.tfinger.fingerID != touchFinger)) return false;
        if (e.type == SDL_EVENT_FINGER_DOWN && touchActive) return true;
        const auto dimensions = aurora::rmlui::get_context()->GetDimensions();
        out.type = SAB_UI_MOUSE_MOVE;
        out.x = e.tfinger.x * dimensions.x; out.y = e.tfinger.y * dimensions.y;
        const bool consumed = callbacks.event(callbacks.userdata, &out) != 0;
        if (!consumed) { touchActive = false; return false; }
        if (e.type == SDL_EVENT_FINGER_DOWN) {
            touchActive = true; touchScrolled = false; touchFinger = e.tfinger.fingerID;
            touchX = out.x; touchY = lastY = out.y;
        } else if (e.type == SDL_EVENT_FINGER_MOTION) {
            if (std::abs(out.x-touchX) > 12.f || std::abs(out.y-touchY) > 12.f) touchScrolled = true;
            if (touchScrolled) {
                const float y = out.y;
                out.type = SAB_UI_MOUSE_WHEEL; out.x = 0; out.y = (y-lastY)/32.f;
                callbacks.event(callbacks.userdata, &out); lastY = y;
            }
        } else {
            touchActive = false;
            if (e.type == SDL_EVENT_FINGER_UP && !touchScrolled) {
                out.x = touchX; out.y = touchY; out.button = SDL_BUTTON_LEFT;
                out.type = SAB_UI_MOUSE_DOWN; callbacks.event(callbacks.userdata, &out);
                out.type = SAB_UI_MOUSE_UP; callbacks.event(callbacks.userdata, &out);
            }
        }
        return true;
    }
    const bool touchMouse = (e.type == SDL_EVENT_MOUSE_MOTION && e.motion.which == SDL_TOUCH_MOUSEID)
        || ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP)
            && e.button.which == SDL_TOUCH_MOUSEID);
    if (touchMouse) {
        out.type = SAB_UI_MOUSE_MOVE;
        if (e.type == SDL_EVENT_MOUSE_MOTION) mousePosition(e.motion.x,e.motion.y,out);
        else mousePosition(e.button.x,e.button.y,out);
        return callbacks.event(callbacks.userdata, &out) != 0;
    }
#endif
    switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
        out.type = e.type == SDL_EVENT_KEY_DOWN ? SAB_UI_KEY_DOWN : SAB_UI_KEY_UP;
        out.keycode = uint32_t(e.key.key); out.scancode = int32_t(e.key.scancode);
        out.modifiers = uint16_t(e.key.mod); out.repeat = e.key.repeat;
        break;
    case SDL_EVENT_MOUSE_MOTION:
        out.type = SAB_UI_MOUSE_MOVE; mousePosition(e.motion.x, e.motion.y, out); break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
        out.type = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? SAB_UI_MOUSE_DOWN : SAB_UI_MOUSE_UP;
        out.button = e.button.button; mousePosition(e.button.x, e.button.y, out); break;
    case SDL_EVENT_MOUSE_WHEEL:
        out.type = SAB_UI_MOUSE_WHEEL;
        out.x = e.wheel.x; out.y = e.wheel.y;
        if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) { out.x = -out.x; out.y = -out.y; }
        break;
    case SDL_EVENT_TEXT_INPUT: out.type = SAB_UI_TEXT; out.text = e.text.text; break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        out.type = SAB_UI_PAD_AXIS; out.which = int32_t(e.gaxis.which);
        out.button = e.gaxis.axis; out.value = e.gaxis.value; break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        out.type = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? SAB_UI_PAD_DOWN : SAB_UI_PAD_UP;
        out.which = int32_t(e.gbutton.which); out.button = e.gbutton.button; break;
    case SDL_EVENT_WINDOW_FOCUS_LOST: out.type = SAB_UI_FOCUS_LOST; break;
    case SDL_EVENT_WINDOW_MOUSE_LEAVE: out.type = SAB_UI_MOUSE_LEAVE; break;
    default: return false;
    }
    const bool consumed = callbacks.event(callbacks.userdata, &out) != 0;
    return out.type == SAB_UI_FOCUS_LOST ? false : consumed;
}

void sab_ui_set_effects(int fxaa, float gamma) {
    sab_gx_sync();
    if (effectFxaa != (fxaa != 0) || effectGamma != gamma) {
        effectFxaa = fxaa != 0; effectGamma = gamma; effectDirty = true;
    }
}

void sab_ui_overlay_draw() {
    sab_gx_sync();
    if (!overlayVisible) return;
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::SetNextWindowPos(ImVec2(12,12), ImGuiCond_Always);
    if (ImGui::Begin("Aurora statistics", nullptr, ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoSavedSettings)) {
        const float fps = ImGui::GetIO().Framerate;
        ImGui::Text("Aurora | %.1f FPS | %.2f ms/frame",fps,fps>0.f?1000.f/fps:0.f);
        if (const auto* s = aurora_get_stats()) {
            ImGui::Text("Draws: %u | merged: %u",s->drawCallCount,s->mergedDrawCallCount);
            ImGui::Text("Pipelines: %u ready | %u queued",s->createdPipelines,s->queuedPipelines);
            ImGui::Text("Texture upload: %.2f MiB",s->lastTextureUploadSize/1048576.f);
        }
    }
    ImGui::End();
}

extern "C" {
void sab_set_overlay(int visible) { sab_gx_sync(); overlayVisible = visible != 0; }
void sab_set_menu_paused(int paused) { sab_gx_sync(); aurora::rmlui::set_game_frame_frozen(paused != 0); }
int sab_ui_initialize(const char* version, const SabUiCallbacks* input) {
    sab_gx_sync();
    auto* context = aurora::rmlui::get_context();
    if (!context || !input || input->abiVersion != SAB_UI_ABI_VERSION) return 0;
    if (!version || Rml::GetVersion() != version) {
        std::fprintf(stderr, "[aurora-menu] RmlUi versions differ: bridge %s, host %s\n",
                     Rml::GetVersion().c_str(), version ? version : "unknown");
        return 0;
    }
    callbacks = *input;
    if (!portalDocument) {
        Rml::Factory::RegisterElementInstancer("sms-ui-portal", &portalInstancer);
        portalDocument = context->LoadDocumentFromMemory(
            "<rml><head><style>body{margin:0;width:100%;height:100%;pointer-events:none;}"
            "sms-ui-portal{display:block;width:100%;height:100%;pointer-events:none;}"
            "</style></head><body><sms-ui-portal/></body></rml>");
        if (!portalDocument) { callbacks = {}; return 0; }
        portalDocument->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    }
    return 1;
}
void sab_ui_shutdown(void) {
    sab_gx_sync();
    callbacks = {};
    if (presentEffect) { renderer()->ReleaseFilter(presentEffect); presentEffect = 0; }
    effectDirty = true;
    if (portalDocument) { portalDocument->Close(); portalDocument = nullptr; }
    for (auto*& cursor : cursors) { if (cursor) SDL_DestroyCursor(cursor); cursor = nullptr; }
}
double sab_ui_time(void) { sab_gx_sync(); return double(SDL_GetTicksNS()) / 1e9; }
void sab_ui_cursor(const char* name) {
    sab_gx_sync();
    SDL_SystemCursor kind = SDL_SYSTEM_CURSOR_DEFAULT;
    if (name && !std::strcmp(name, "pointer")) kind = SDL_SYSTEM_CURSOR_POINTER;
    else if (name && !std::strcmp(name, "text")) kind = SDL_SYSTEM_CURSOR_TEXT;
    else if (name && !std::strcmp(name, "move")) kind = SDL_SYSTEM_CURSOR_MOVE;
    else if (name && !std::strcmp(name, "cross")) kind = SDL_SYSTEM_CURSOR_CROSSHAIR;
    else if (name && !std::strcmp(name, "unavailable")) kind = SDL_SYSTEM_CURSOR_NOT_ALLOWED;
    else if (name && (!std::strcmp(name,"resize")||!std::strcmp(name,"nwse-resize"))) kind = SDL_SYSTEM_CURSOR_NWSE_RESIZE;
    else if (name && !std::strcmp(name,"nesw-resize")) kind = SDL_SYSTEM_CURSOR_NESW_RESIZE;
    else if (name && !std::strcmp(name,"ew-resize")) kind = SDL_SYSTEM_CURSOR_EW_RESIZE;
    else if (name && !std::strcmp(name,"ns-resize")) kind = SDL_SYSTEM_CURSOR_NS_RESIZE;
    if (!cursors[kind]) cursors[kind] = SDL_CreateSystemCursor(kind);
    if (cursors[kind]) SDL_SetCursor(cursors[kind]);
}
void sab_ui_clipboard_set(const char* text) { sab_gx_sync(); SDL_SetClipboardText(text ? text : ""); }
uint32_t sab_ui_clipboard_get(char* text, uint32_t capacity) {
    sab_gx_sync();
    char* clipboard = SDL_GetClipboardText();
    const uint32_t size = clipboard ? uint32_t(std::strlen(clipboard) + 1) : 1;
    if (text && capacity) {
        const uint32_t copy = std::min(size - 1, capacity - 1);
        if (clipboard) std::memcpy(text, clipboard, copy);
        text[copy] = 0;
    }
    SDL_free(clipboard);
    return size;
}
void sab_ui_keyboard(int enabled, float x, float y, float height) {
    sab_gx_sync();
    if (SDL_Window* w = window()) {
        if (enabled) {
            // Rml caret coordinates are content pixels; SDL IME expects
            // window coordinates (possibly a different scale and letterbox).
            int width=0,heightPixels=0,pixelWidth=0,pixelHeight=0;
            SDL_GetWindowSize(w,&width,&heightPixels);
            SDL_GetWindowSizeInPixels(w,&pixelWidth,&pixelHeight);
            if(auto* context=aurora::rmlui::get_context()) {
                const auto size=context->GetDimensions();
                const auto& source=aurora::webgpu::present_source();
                const auto viewport=aurora::webgpu::calculate_present_viewport(
                    std::max(pixelWidth,1),std::max(pixelHeight,1),std::max(source.size.width,1u),std::max(source.size.height,1u));
                x=(viewport.left+x*viewport.width/std::max(size.x,1))*width/std::max(pixelWidth,1);
                y=(viewport.top+y*viewport.height/std::max(size.y,1))*heightPixels/std::max(pixelHeight,1);
                height=height*viewport.height/std::max(size.y,1)*heightPixels/std::max(pixelHeight,1);
            }
            SDL_Rect area{int(x), int(y), 1, std::max(1, int(height))};
            SDL_SetTextInputArea(w, &area, 0); SDL_StartTextInput(w);
        } else SDL_StopTextInput(w);
    }
}
void sab_ui_request_quit(void) { sab_gx_sync(); SDL_Event e{}; e.type = SDL_EVENT_QUIT; SDL_PushEvent(&e); }
uint64_t sab_ui_geometry_create(const SabUiVertex* input, uint32_t count, const int32_t* indices, uint32_t indexCount) {
    sab_gx_sync();
    std::vector<Rml::Vertex> vertices(count);
    for (uint32_t i = 0; i < count; ++i) {
        const auto& v = input[i];
        vertices[i] = {{v.x, v.y}, {v.color[0], v.color[1], v.color[2], v.color[3]}, {v.u, v.v}};
    }
    static_assert(sizeof(int) == sizeof(int32_t));
    return renderer()->CompileGeometry({vertices.data(), vertices.size()}, {indices, indexCount});
}
void sab_ui_geometry_draw(uint64_t g, float x, float y, uint64_t t) { sab_gx_sync(); renderer()->RenderGeometry(g, {x,y}, t); }
void sab_ui_geometry_release(uint64_t g) { sab_gx_sync(); renderer()->ReleaseGeometry(g); }
uint64_t sab_ui_texture_create(const uint8_t* rgba, int w, int h) {
    sab_gx_sync();
    return renderer()->GenerateTexture({rgba, size_t(w)*size_t(h)*4}, {w,h});
}
void sab_ui_texture_release(uint64_t t) { sab_gx_sync(); renderer()->ReleaseTexture(t); }
void sab_ui_scissor_enable(int on) { sab_gx_sync(); renderer()->EnableScissorRegion(on != 0); }
void sab_ui_scissor_set(int l, int t, int r, int b) { sab_gx_sync(); renderer()->SetScissorRegion(Rml::Rectanglei::FromPositionSize({l,t},{r-l,b-t})); }
void sab_ui_clip_enable(int on) { sab_gx_sync(); renderer()->EnableClipMask(on != 0); }
void sab_ui_clip_draw(int op, uint64_t g, float x, float y) { sab_gx_sync(); renderer()->RenderToClipMask(Rml::ClipMaskOperation(op),g,{x,y}); }
void sab_ui_transform(const float* rows) {
    sab_gx_sync();
    if (!rows) { renderer()->SetTransform(nullptr); return; }
    Rml::Matrix4f m;
    for (int i=0;i<4;++i) m.SetRow(i,{rows[i*4],rows[i*4+1],rows[i*4+2],rows[i*4+3]});
    renderer()->SetTransform(&m);
}
uint64_t sab_ui_layer_push(void) { sab_gx_sync(); return renderer()->PushLayer(); }
void sab_ui_layer_composite(uint64_t s, uint64_t d, int replace, const uint64_t* filters, uint32_t count) {
    sab_gx_sync();
    std::vector<Rml::CompiledFilterHandle> handles;
    if (count) handles.assign(filters, filters+count);
    renderer()->CompositeLayers(s,d,replace ? Rml::BlendMode::Replace : Rml::BlendMode::Blend,{handles.data(),handles.size()});
}
void sab_ui_layer_pop(void) { sab_gx_sync(); renderer()->PopLayer(); }
uint64_t sab_ui_layer_texture(void) { sab_gx_sync(); return renderer()->SaveLayerAsTexture(); }
uint64_t sab_ui_layer_mask(void) { sab_gx_sync(); return renderer()->SaveLayerAsMaskImage(); }
uint64_t sab_ui_filter_create(const char* name, const SabUiParameter* p, uint32_t count) { sab_gx_sync(); return renderer()->CompileFilter(name,dictionary(p,count)); }
void sab_ui_filter_release(uint64_t f) { sab_gx_sync(); renderer()->ReleaseFilter(f); }
uint64_t sab_ui_shader_create(const char* name, const SabUiParameter* p, uint32_t count) { sab_gx_sync(); return renderer()->CompileShader(name,dictionary(p,count)); }
void sab_ui_shader_draw(uint64_t s, uint64_t g, float x, float y, uint64_t t) { sab_gx_sync(); renderer()->RenderShader(s,g,{x,y},t); }
void sab_ui_shader_release(uint64_t s) { sab_gx_sync(); renderer()->ReleaseShader(s); }
}
