#include "partyboard_backend.h"
#include "sms_aurora_ui.h"
#include "RmlUi_Platform_SDL.h"
#include <RmlUi/Core.h>
#include <RmlUi/Core/DecorationTypes.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <set>
#include <vector>
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

extern "C" void GXPC_MenuDraw(int, int);
extern "C" int GXPC_MenuProcessEvent(const void*);

namespace sms_partyboard {
namespace {
Rml::Context* context = nullptr;
std::set<Rml::Input::KeyIdentifier> heldKeys;
uint32_t heldMouseButtons = 0;
bool resettingInput = false;
int stickX = 0, stickY = 0;
Rml::Input::KeyIdentifier stickDirection = Rml::Input::KI_UNKNOWN;
double nextRepeat = 0;

class System final : public Rml::SystemInterface {
public:
    double GetElapsedTime() override { return sab_ui_time(); }
    void SetMouseCursor(const Rml::String& name) override { sab_ui_cursor(name.c_str()); }
    void SetClipboardText(const Rml::String& text) override { sab_ui_clipboard_set(text.c_str()); }
    void GetClipboardText(Rml::String& text) override {
        const uint32_t count = sab_ui_clipboard_get(nullptr, 0);
        std::vector<char> buffer(std::max(count, 1u));
        sab_ui_clipboard_get(buffer.data(), uint32_t(buffer.size()));
        text = buffer.data();
    }
    void ActivateKeyboard(Rml::Vector2f caret, float lineHeight) override { sab_ui_keyboard(1, caret.x, caret.y, lineHeight); }
    void DeactivateKeyboard() override { sab_ui_keyboard(0, 0, 0, 0); }
};

struct Parameters {
    std::vector<SabUiParameter> values;
    std::vector<std::vector<SabUiColorStop>> stops;
    explicit Parameters(const Rml::Dictionary& dictionary) : stops(dictionary.size()) {
        values.reserve(dictionary.size());
        size_t slot = 0;
        for (const auto& entry : dictionary) {
            SabUiParameter p{};
            p.name = entry.first.c_str();
            const auto& v = entry.second;
            switch (v.GetType()) {
            case Rml::Variant::BOOL: p.type = SAB_UI_BOOLEAN; p.value[0] = v.Get<bool>() ? 1.f : 0.f; break;
            case Rml::Variant::FLOAT: case Rml::Variant::DOUBLE:
            case Rml::Variant::INT: case Rml::Variant::UINT:
                p.type = SAB_UI_NUMBER; p.value[0] = v.Get<float>(); break;
            case Rml::Variant::VECTOR2: {
                const auto vector = v.Get<Rml::Vector2f>();
                p.type = SAB_UI_VECTOR2; p.value[0] = vector.x; p.value[1] = vector.y; break;
            }
            case Rml::Variant::COLOURB: {
                const auto color = v.Get<Rml::Colourb>();
                p.type = SAB_UI_COLOR;
                p.value[0] = color.red; p.value[1] = color.green; p.value[2] = color.blue; p.value[3] = color.alpha;
                break;
            }
            case Rml::Variant::COLORSTOPLIST:
                p.type = SAB_UI_STOPS;
                for (const auto& source : v.GetReference<Rml::ColorStopList>())
                    stops[slot].push_back({{source.color.red, source.color.green, source.color.blue, source.color.alpha},
                                           source.position.number, uint32_t(source.position.unit)});
                p.stops = stops[slot].data(); p.stopCount = uint32_t(stops[slot].size());
                ++slot;
                break;
            default:
                std::fprintf(stderr, "[aurora-menu] unsupported render parameter: %s\n", entry.first.c_str());
                continue;
            }
            values.push_back(p);
        }
    }
};

class Renderer final : public Rml::RenderInterface {
public:
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override {
        std::vector<SabUiVertex> converted;
        converted.reserve(vertices.size());
        for (const auto& v : vertices)
            converted.push_back({v.position.x,v.position.y,{v.colour.red,v.colour.green,v.colour.blue,v.colour.alpha},v.tex_coord.x,v.tex_coord.y});
        static_assert(sizeof(int) == sizeof(int32_t));
        return sab_ui_geometry_create(converted.data(), uint32_t(converted.size()), indices.data(), uint32_t(indices.size()));
    }
    void RenderGeometry(Rml::CompiledGeometryHandle g, Rml::Vector2f t, Rml::TextureHandle image) override { sab_ui_geometry_draw(g,t.x,t.y,image); }
    void ReleaseGeometry(Rml::CompiledGeometryHandle g) override { sab_ui_geometry_release(g); }
    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& path) override {
        auto* files = Rml::GetFileInterface();
        const auto file = files->Open(path);
        if (!file) return 0;
        files->Seek(file,0,SEEK_END); const size_t size = files->Tell(file); files->Seek(file,0,SEEK_SET);
        if (size > size_t(std::numeric_limits<int>::max())) { files->Close(file); return 0; }
        std::vector<unsigned char> bytes(size);
        const size_t read = files->Read(bytes.data(),bytes.size(),file); files->Close(file);
        if (read != size || !size) return 0;
        int width=0,height=0,channels=0;
        unsigned char* pixels = stbi_load_from_memory(bytes.data(),int(size),&width,&height,&channels,4);
        if (!pixels) { std::fprintf(stderr,"[aurora-menu] texture failed: %s\n",path.c_str()); return 0; }
        const size_t count = size_t(width)*size_t(height)*4;
        for (size_t i=0;i<count;i+=4)
            for (int c=0;c<3;++c) pixels[i+c] = unsigned(pixels[i+c])*unsigned(pixels[i+3])/255;
        dimensions={width,height};
        const auto result = GenerateTexture({pixels,count},dimensions);
        stbi_image_free(pixels);
        return result;
    }
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i size) override {
        return sab_ui_texture_create(source.data(),size.x,size.y);
    }
    void ReleaseTexture(Rml::TextureHandle t) override { sab_ui_texture_release(t); }
    void EnableScissorRegion(bool on) override { sab_ui_scissor_enable(on); }
    void SetScissorRegion(Rml::Rectanglei r) override { sab_ui_scissor_set(r.Left(),r.Top(),r.Right(),r.Bottom()); }
    void EnableClipMask(bool on) override { sab_ui_clip_enable(on); }
    void RenderToClipMask(Rml::ClipMaskOperation op,Rml::CompiledGeometryHandle g,Rml::Vector2f t) override { sab_ui_clip_draw(int(op),g,t.x,t.y); }
    void SetTransform(const Rml::Matrix4f* transform) override {
        if (!transform) { sab_ui_transform(nullptr); return; }
        float rows[16];
        for(int i=0;i<4;++i) { const auto row=transform->GetRow(i); for(int j=0;j<4;++j) rows[i*4+j]=row[j]; }
        sab_ui_transform(rows);
    }
    Rml::LayerHandle PushLayer() override { return sab_ui_layer_push(); }
    void CompositeLayers(Rml::LayerHandle s,Rml::LayerHandle d,Rml::BlendMode mode,Rml::Span<const Rml::CompiledFilterHandle> filters) override {
        std::vector<uint64_t> values(filters.begin(),filters.end());
        sab_ui_layer_composite(s,d,mode==Rml::BlendMode::Replace,values.data(),uint32_t(values.size()));
    }
    void PopLayer() override { sab_ui_layer_pop(); }
    Rml::TextureHandle SaveLayerAsTexture() override { return sab_ui_layer_texture(); }
    Rml::CompiledFilterHandle SaveLayerAsMaskImage() override { return sab_ui_layer_mask(); }
    Rml::CompiledFilterHandle CompileFilter(const Rml::String& name,const Rml::Dictionary& dictionary) override {
        Parameters p(dictionary); return sab_ui_filter_create(name.c_str(),p.values.data(),uint32_t(p.values.size()));
    }
    void ReleaseFilter(Rml::CompiledFilterHandle f) override { sab_ui_filter_release(f); }
    Rml::CompiledShaderHandle CompileShader(const Rml::String& name,const Rml::Dictionary& dictionary) override {
        Parameters p(dictionary); return sab_ui_shader_create(name.c_str(),p.values.data(),uint32_t(p.values.size()));
    }
    void RenderShader(Rml::CompiledShaderHandle s,Rml::CompiledGeometryHandle g,Rml::Vector2f t,Rml::TextureHandle image) override { sab_ui_shader_draw(s,g,t.x,t.y,image); }
    void ReleaseShader(Rml::CompiledShaderHandle s) override { sab_ui_shader_release(s); }
};
std::unique_ptr<System> system;
std::unique_ptr<Renderer> renderer;

int modifiers(uint16_t m) {
    return ((m&0x0003) ? Rml::Input::KM_SHIFT : 0) | ((m&0x00c0) ? Rml::Input::KM_CTRL : 0) |
           ((m&0x0300) ? Rml::Input::KM_ALT : 0) | ((m&0x0c00) ? Rml::Input::KM_META : 0) |
           ((m&0x1000) ? Rml::Input::KM_NUMLOCK : 0) | ((m&0x2000) ? Rml::Input::KM_CAPSLOCK : 0);
}
Rml::Input::KeyIdentifier controllerKey(uint8_t button) {
    using namespace Rml::Input;
    switch(button) {
    case 0:return KI_RETURN; case 1:return KI_ESCAPE; case 9:case 10:return KI_TAB;
    case 11:return KI_UP;case 12:return KI_DOWN;case 13:return KI_LEFT;case 14:return KI_RIGHT;
    default:return KI_UNKNOWN;
    }
}
void updateStick() {
    const int magnitude=std::max(std::abs(stickX),std::abs(stickY));
    auto direction=stickDirection;
    if(magnitude<9000) direction=Rml::Input::KI_UNKNOWN;
    else if(magnitude>=14000)
        direction=std::abs(stickX)>std::abs(stickY) ? (stickX<0?Rml::Input::KI_LEFT:Rml::Input::KI_RIGHT) :
                                                               (stickY<0?Rml::Input::KI_UP:Rml::Input::KI_DOWN);
    const bool changed=direction!=stickDirection; stickDirection=direction;
    if(direction==Rml::Input::KI_UNKNOWN) { nextRepeat=0; return; }
    const double now=sab_ui_time();
    if(changed||now>=nextRepeat) {
        context->ProcessKeyDown(direction,0);context->ProcessKeyUp(direction,0);
        nextRepeat=now+(changed?0.300:0.140);
    }
}
void drawCallback(void*,int width,int height,float density) {
    if(context) context->SetDensityIndependentPixelRatio(density);
    GXPC_MenuDraw(width,height);
}
int eventCallback(void*,const SabUiEvent* event) { return GXPC_MenuProcessEvent(event); }
}

bool Initialize(void*,void*,int width,int height) {
    if(context) return true;
    system=std::make_unique<System>(); renderer=std::make_unique<Renderer>();
    Rml::SetSystemInterface(system.get());Rml::SetRenderInterface(renderer.get());
    if(!Rml::Initialise()) { renderer.reset();system.reset();return false; }
    context=Rml::CreateContext("sms-partyboard-pal",{std::max(width,1),std::max(height,1)});
    SabUiCallbacks callbacks{SAB_UI_ABI_VERSION,nullptr,drawCallback,eventCallback};
    if(!context||!sab_ui_initialize(Rml::GetVersion().c_str(),&callbacks)) { Shutdown();return false; }
    std::fprintf(stderr,"[partyboard] Aurora WebGPU backend initialized\n");
    return true;
}
Rml::Context* GetContext() { return context; }
void ResetInput() {
    stickX=stickY=0;stickDirection=Rml::Input::KI_UNKNOWN;nextRepeat=0;
    if(!context || resettingInput) return;
    resettingInput=true;
    const auto keys=std::move(heldKeys);
    heldKeys.clear();
    const uint32_t buttons=heldMouseButtons;
    heldMouseButtons=0;
    // RmlUi emits Click from MouseButtonUp before clearing its active chain.
    // Leave first, and only release buttons actually held, so cancel/resume
    // cannot synthesize a click or recursively re-enter the menu callback.
    context->ProcessMouseLeave();
    for(auto key:keys) context->ProcessKeyUp(key,0);
    for(int i=0;i<3;++i) if(buttons&(1u<<i))context->ProcessMouseButtonUp(i,0);
    resettingInput=false;
}
bool ProcessEvent(const void* raw) {
    if(!context||!raw) return false;
    const auto& e=*static_cast<const SabUiEvent*>(raw);
    const int mods=modifiers(e.modifiers);
    switch(e.type) {
    case SAB_UI_KEY_DOWN:case SAB_UI_KEY_UP: {
        const auto key=RmlSDL::ConvertKey(int(e.keycode));
        if(e.type==SAB_UI_KEY_DOWN) { heldKeys.insert(key);context->ProcessKeyDown(key,mods); }
        else { heldKeys.erase(key);context->ProcessKeyUp(key,mods); }
        break;
    }
    case SAB_UI_MOUSE_MOVE:context->ProcessMouseMove(int(std::floor(e.x)),int(std::floor(e.y)),mods);break;
    case SAB_UI_MOUSE_DOWN:case SAB_UI_MOUSE_UP: {
        context->ProcessMouseMove(int(std::floor(e.x)),int(std::floor(e.y)),mods);
        const int button=RmlSDL::ConvertMouseButton(e.button);
        if(button<0||button>=3)break;
        if(e.type==SAB_UI_MOUSE_DOWN) {
            heldMouseButtons|=1u<<button;
            context->ProcessMouseButtonDown(button,mods);
        } else {
            heldMouseButtons&=~(1u<<button);
            context->ProcessMouseButtonUp(button,mods);
        }
        break;
    }
    case SAB_UI_MOUSE_WHEEL:context->ProcessMouseWheel(Rml::Vector2f(-e.x,-e.y),mods);break;
    case SAB_UI_TEXT:if(e.text) context->ProcessTextInput(e.text);break;
    case SAB_UI_PAD_AXIS:if(e.button==0)stickX=e.value;else if(e.button==1)stickY=e.value;break;
    case SAB_UI_PAD_DOWN:case SAB_UI_PAD_UP: {
        const auto key=controllerKey(e.button);const int shift=e.button==9?Rml::Input::KM_SHIFT:0;
        if(key!=Rml::Input::KI_UNKNOWN) {
            if(e.type==SAB_UI_PAD_DOWN){heldKeys.insert(key);context->ProcessKeyDown(key,shift);}
            else{heldKeys.erase(key);context->ProcessKeyUp(key,shift);}
        }
        break;
    }
    case SAB_UI_FOCUS_LOST:ResetInput();break;
    case SAB_UI_MOUSE_LEAVE:context->ProcessMouseLeave();break;
    }
    return true;
}
void Render(int width,int height) {
    if(!context||width<1||height<1)return;
    context->SetDimensions({width,height});updateStick();context->Update();context->Render();
}
void Shutdown() {
    if(context) { ResetInput();Rml::RemoveContext("sms-partyboard-pal");context=nullptr; }
    if(renderer) Rml::Shutdown();
    sab_ui_shutdown();renderer.reset();system.reset();
}
}
