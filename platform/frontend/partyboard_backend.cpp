#include "partyboard_backend.h"
#include "RmlUi_Platform_SDL.h"
#include "RmlUi_Renderer_GL3.h"
#include "RmlUi_Include_GL3.h"
#include <RmlUi/Core.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <vector>

// The GX texture loader has its own stb implementation; keep this one private.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

namespace sms_partyboard {
namespace {
SDL_Window* window = nullptr;
Rml::Context* context = nullptr;
std::unique_ptr<SystemInterface_SDL> system;

// The upstream renderer restores blend/stencil/scissor state. Bindings are
// additional GX state that it does not restore (including the EFB attachment).
struct GlBindings {
    GLint drawFramebuffer = 0, readFramebuffer = 0, renderbuffer = 0;
    GLint program = 0, vao = 0, arrayBuffer = 0, activeTexture = 0;
    GLint viewport[4] = {}, polygonMode[2] = {};
    GLint textures[2] = {}, samplers[2] = {};
    GLboolean framebufferSrgb = GL_FALSE;
    GlBindings() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_POLYGON_MODE, polygonMode);
        framebufferSrgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
        for (int unit = 0; unit < 2; ++unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[unit]);
            glGetIntegerv(GL_SAMPLER_BINDING, &samplers[unit]);
        }
        glActiveTexture(activeTexture);
    }
    ~GlBindings() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
        for (int unit = 0; unit < 2; ++unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, textures[unit]);
            glBindSampler(unit, samplers[unit]);
        }
        glActiveTexture(activeTexture);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
        if (framebufferSrgb) glEnable(GL_FRAMEBUFFER_SRGB);
        else glDisable(GL_FRAMEBUFFER_SRGB);
    }
};

class TextureRenderer : public RenderInterface_GL3 {
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override {
        Rml::FileInterface* files = Rml::GetFileInterface();
        const Rml::FileHandle file = files->Open(source);
        if (!file) return {};
        files->Seek(file, 0, SEEK_END);
        const size_t length = files->Tell(file);
        files->Seek(file, 0, SEEK_SET);
        if (length > size_t(std::numeric_limits<int>::max())) { files->Close(file); return {}; }
        std::vector<unsigned char> bytes(length);
        const size_t read = files->Read(bytes.data(), length, file);
        files->Close(file);
        if (read != length || bytes.empty()) return {};
        int width = 0, height = 0, channels = 0;
        unsigned char* pixels = stbi_load_from_memory(bytes.data(), int(length), &width, &height, &channels, 4);
        if (!pixels) { fprintf(stderr, "[partyboard] texture failed: %s\n", source.c_str()); return {}; }
        const size_t count = size_t(width) * size_t(height) * 4;
        for (size_t pixel = 0; pixel < count; pixel += 4)
            for (size_t color = 0; color < 3; ++color)
                pixels[pixel + color] = unsigned(pixels[pixel + color]) * unsigned(pixels[pixel + 3]) / 255;
        dimensions = {width, height};
        const Rml::TextureHandle result = GenerateTexture({pixels, count}, dimensions);
        stbi_image_free(pixels);
        return result;
    }
};
std::unique_ptr<TextureRenderer> renderer;
Sint16 stickX = 0, stickY = 0;
Rml::Input::KeyIdentifier stickDirection = Rml::Input::KI_UNKNOWN;
Uint32 stickNextRepeat = 0;

Rml::Input::KeyIdentifier ControllerKey(Uint8 button) {
    switch (button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return Rml::Input::KI_UP;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return Rml::Input::KI_DOWN;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return Rml::Input::KI_LEFT;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Rml::Input::KI_RIGHT;
    case SDL_CONTROLLER_BUTTON_A: return Rml::Input::KI_RETURN;
    case SDL_CONTROLLER_BUTTON_B: return Rml::Input::KI_ESCAPE;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Rml::Input::KI_TAB;
    default: return Rml::Input::KI_UNKNOWN;
    }
}

void ResetStick() {
    stickX = stickY = 0;
    stickDirection = Rml::Input::KI_UNKNOWN;
    stickNextRepeat = 0;
}

void UpdateStickNavigation() {
    // Hysteresis prevents noise near the dead zone. Holding the stick repeats
    // at a bounded rate even when SDL reports no further axis changes.
    const int x = int(stickX), y = int(stickY);
    const int magnitude = std::max(std::abs(x), std::abs(y));
    Rml::Input::KeyIdentifier direction = stickDirection;
    if (magnitude < 9000) direction = Rml::Input::KI_UNKNOWN;
    else if (magnitude >= 14000)
        direction = std::abs(x) > std::abs(y) ? (x < 0 ? Rml::Input::KI_LEFT : Rml::Input::KI_RIGHT) :
                                              (y < 0 ? Rml::Input::KI_UP : Rml::Input::KI_DOWN);
    const Uint32 now = SDL_GetTicks();
    const bool changed = direction != stickDirection;
    stickDirection = direction;
    if (direction == Rml::Input::KI_UNKNOWN) { stickNextRepeat = 0; return; }
    if (changed || Sint32(now - stickNextRepeat) >= 0) {
        context->ProcessKeyDown(direction, 0);
        context->ProcessKeyUp(direction, 0);
        stickNextRepeat = now + (changed ? 300 : 140);
    }
}

void Resize(int width, int height) {
    width = std::max(1, width); height = std::max(1, height);
    context->SetDimensions({width, height});
    int logicalWidth = 0, logicalHeight = 0;
    SDL_GetWindowSize(window, &logicalWidth, &logicalHeight);
    context->SetDensityIndependentPixelRatio(logicalHeight > 0 ? float(height) / logicalHeight : 1.f);
    renderer->SetViewport(width, height);
}
}

bool Initialize(void* gameWindow, void* glContext, int width, int height) {
    if (context) return true;
    if (!gameWindow || !glContext) return false;
    window = static_cast<SDL_Window*>(gameWindow);
    if (SDL_GL_GetCurrentWindow() != window || SDL_GL_GetCurrentContext() != glContext) {
        fprintf(stderr, "[partyboard] initialization requires the game's current GL context\n");
        window = nullptr;
        return false;
    }
    Rml::String message;
    if (!RmlGL3::Initialize(&message)) {
        fprintf(stderr, "[partyboard] %s\n", message.c_str());
        window = nullptr;
        return false;
    }
    GlBindings bindings;
    system.reset(new SystemInterface_SDL(window));
    renderer.reset(new TextureRenderer);
    if (!static_cast<bool>(*renderer)) {
        renderer.reset(); system.reset(); window = nullptr;
        return false;
    }
    Rml::SetSystemInterface(system.get());
    Rml::SetRenderInterface(renderer.get());
    if (!Rml::Initialise()) {
        Rml::SetSystemInterface(nullptr); Rml::SetRenderInterface(nullptr);
        renderer.reset(); system.reset(); window = nullptr;
        return false;
    }
    context = Rml::CreateContext("sms-partyboard-pal", {std::max(1, width), std::max(1, height)});
    if (!context) {
        Rml::Shutdown(); renderer.reset(); system.reset(); window = nullptr;
        return false;
    }
    Resize(width, height);
    fprintf(stderr, "[partyboard] RmlUi GL3 backend initialized: %s\n", message.c_str());
    return true;
}

Rml::Context* GetContext() { return context; }

void ResetInput() {
    ResetStick();
    if (!context) return;
    static const Rml::Input::KeyIdentifier keys[] = {
        Rml::Input::KI_UP, Rml::Input::KI_DOWN, Rml::Input::KI_LEFT,
        Rml::Input::KI_RIGHT, Rml::Input::KI_RETURN, Rml::Input::KI_ESCAPE,
        Rml::Input::KI_TAB
    };
    for (auto key : keys) context->ProcessKeyUp(key, 0);
}

bool ProcessEvent(const void* event) {
    if (!context || !event) return false;
    SDL_Event translated = *static_cast<const SDL_Event*>(event);
    if (translated.type == SDL_CONTROLLERBUTTONDOWN || translated.type == SDL_CONTROLLERBUTTONUP) {
        const Rml::Input::KeyIdentifier key = ControllerKey(translated.cbutton.button);
        const int modifiers = translated.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSHOULDER ?
                              Rml::Input::KM_SHIFT : 0;
        if (key != Rml::Input::KI_UNKNOWN) {
            if (translated.type == SDL_CONTROLLERBUTTONDOWN) context->ProcessKeyDown(key, modifiers);
            else context->ProcessKeyUp(key, modifiers);
        }
        return true;
    }
    if (translated.type == SDL_CONTROLLERAXISMOTION) {
        if (translated.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) stickX = translated.caxis.value;
        else if (translated.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) stickY = translated.caxis.value;
        else return true;
        UpdateStickNavigation();
        return true;
    }
    if (translated.type == SDL_CONTROLLERDEVICEREMOVED ||
        (translated.type == SDL_WINDOWEVENT && (translated.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                                               translated.window.event == SDL_WINDOWEVENT_HIDDEN)))
        ResetStick();
    // SDL2 backend coordinates are logical pixels, while GX/Rml renders to
    // drawable pixels. SDL3's backend already performs its own conversion.
    int logicalWidth = 0, logicalHeight = 0;
    SDL_GetWindowSize(window, &logicalWidth, &logicalHeight);
    const Rml::Vector2i dimensions = context->GetDimensions();
    if (translated.type == SDL_MOUSEMOTION && logicalWidth > 0 && logicalHeight > 0) {
        translated.motion.x = int(double(translated.motion.x) * dimensions.x / logicalWidth);
        translated.motion.y = int(double(translated.motion.y) * dimensions.y / logicalHeight);
    }
    if (translated.type == SDL_MOUSEBUTTONDOWN || translated.type == SDL_MOUSEBUTTONUP) {
        // SDL button events can arrive without a preceding motion (touchpad or
        // a click immediately after entering); synchronize the pointer first.
        if (logicalWidth > 0 && logicalHeight > 0)
            context->ProcessMouseMove(int(double(translated.button.x) * dimensions.x / logicalWidth),
                                      int(double(translated.button.y) * dimensions.y / logicalHeight),
                                      RmlSDL::GetKeyModifierState());
    }
    const bool consumed = !RmlSDL::InputEventHandler(context, window, translated);
    if (translated.type == SDL_WINDOWEVENT && translated.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        int width = 0, height = 0;
        SDL_GL_GetDrawableSize(window, &width, &height);
        Resize(width, height);
    }
    return consumed;
}

void Render(int width, int height) {
    if (!context || width < 1 || height < 1) return;
    GlBindings bindings;
    Resize(width, height);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    UpdateStickNavigation();
    context->Update();
    renderer->BeginFrame();
    context->Render();
    renderer->EndFrame();
}

void Shutdown() {
    if (!context) return;
    {
        GlBindings bindings;
        Rml::RemoveContext("sms-partyboard-pal");
        context = nullptr;
        ResetStick();
        Rml::Shutdown();
        renderer.reset(); system.reset();
    }
    RmlGL3::Shutdown();
    window = nullptr;
}
}
