#define SMS_AURORA_BUILD
#include "sms_aurora_settings.h"
#include "ui_internal.h"
#include <aurora/gfx.h>
#include <dolphin/vi.h>
#include "internal.hpp"
#include "window.hpp"
#include "webgpu/gpu.hpp"
#include "gfx/common.hpp"
#include <aurora/texture.hpp>
#include "dolphin/vi/vi_internal.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
struct WindowSettings {
    bool pending=false;
    std::string vsync,mode,fullscreenMode;
    int display=0,scale=0;
} windowSettings;
WindowSettings appliedWindow;
bool windowApplied=false;
struct GraphicsSettings {
    bool pending=false;
    int scale=1,msaa=1,fxaa=0,anisotropy=1;
    float gamma=1.f,widescreen=1.f;
    std::string aspect,filter;
} graphicsSettings;
bool packsPending = false, packsEnabled = true;
std::vector<std::string> packPaths;
aurora::texture::ReplacementGroup packs;
}

void sab_apply_pending_settings() {
    if (windowSettings.pending || graphicsSettings.pending || packsPending)
        aurora::gfx::gpu_synchronize();
    if(windowSettings.pending) {
        windowSettings.pending=false;
        auto* w=aurora::window::get_sdl_window();
        const auto& s=windowSettings;
        aurora::g_config.vsync=s.vsync!="off"&&s.vsync!="0";
        aurora::webgpu::set_adaptive_vsync(s.vsync=="adaptive");
        sab_set_vsync(s.vsync!="off"&&s.vsync!="0");
        int count=0;
        SDL_DisplayID* displays=SDL_GetDisplays(&count);
        const auto display=displays&&count ? displays[std::clamp(s.display,0,count-1)] : SDL_GetPrimaryDisplay();
        SDL_free(displays);
        const bool geometryChanged=!windowApplied||s.mode!=appliedWindow.mode||s.fullscreenMode!=appliedWindow.fullscreenMode||
            s.display!=appliedWindow.display||s.scale!=appliedWindow.scale;
        if(w&&geometryChanged) {
            SDL_SetWindowFullscreen(w,false);
            SDL_SetWindowPosition(w,SDL_WINDOWPOS_CENTERED_DISPLAY(display),SDL_WINDOWPOS_CENTERED_DISPLAY(display));
            SDL_SetWindowFullscreenMode(w,nullptr);
            if(s.mode=="fullscreen"&&s.fullscreenMode!="desktop") {
                int width=0,height=0;float rate=0;
                if(std::sscanf(s.fullscreenMode.c_str(),"%dx%d@%f",&width,&height,&rate)>=2&&width>0&&height>0) {
                    SDL_DisplayMode selected{};
                    if(SDL_GetClosestFullscreenDisplayMode(display,width,height,rate,true,&selected))
                        SDL_SetWindowFullscreenMode(w,&selected);
                }
            }
            if(s.mode=="fullscreen"||s.mode=="borderless")SDL_SetWindowFullscreen(w,true);
            else if(s.scale>0) {
                const auto size=aurora::vi::configured_fb_size();
                SDL_SetWindowSize(w,int(size.x)*s.scale,int(size.y)*s.scale);
            }
        }
        appliedWindow=s;windowApplied=true;
    }
    if(graphicsSettings.pending) {
        graphicsSettings.pending=false;
        const auto& s=graphicsSettings;
        auto& gpu=aurora::webgpu::g_graphicsConfig;
        const bool recreate=gpu.msaaSamples!=uint32_t(s.msaa);
        gpu.msaaSamples=uint32_t(s.msaa);
        gpu.textureAnisotropy=uint16_t(s.anisotropy);
        aurora::g_config.msaa=uint32_t(s.msaa);
        aurora::g_config.maxTextureAnisotropy=uint16_t(s.anisotropy);
        VISetFrameBufferScale(float(s.scale));
        const auto base = aurora::vi::configured_fb_size();
        aurora::window::set_frame_buffer_content_aspect(float(base.x)/float(std::max(base.y,1u))*s.widescreen);
        aurora::window::set_frame_buffer_aspect_fit(true);
        aurora::webgpu::set_presentation_mode(s.aspect=="stretch"?1:s.aspect=="integer"?2:0,
            uint32_t(float(base.x)*s.widescreen),base.y);
        aurora::webgpu::set_resampler(s.filter=="nearest"?SAMPLER_NEAREST:SAMPLER_BILINEAR);
        if(recreate) {
            const auto size=aurora::window::get_window_size();
            aurora::webgpu::resize_swapchain(size.fb_width,size.fb_height,size.native_fb_width,size.native_fb_height,true);
        }
        sab_ui_set_effects(s.fxaa,s.gamma);
        std::fprintf(stderr,"[aurora-menu] live graphics: scale %d, MSAA %d, anisotropy %d, FXAA %d, gamma %.2f\n",
                     s.scale,s.msaa,s.anisotropy,s.fxaa,s.gamma);
    }
    if (packsPending) {
        packsPending = false;
        aurora::texture::unregister_replacements(packs);
        packs.registrations.clear();
        if (packsEnabled) {
            int priority=0;
            for (const auto& path : packPaths) {
                const auto root=std::filesystem::u8path(path);
                std::error_code ec;
                if (!std::filesystem::is_directory(root,ec)) continue;
                auto group=aurora::texture::load_replacement_directory(root,{priority++});
                packs.registrations.insert(packs.registrations.end(),group.registrations.begin(),group.registrations.end());
            }
        }
        std::fprintf(stderr,"[aurora-menu] texture packs: %zu registrations (%s)\n",
                     packs.registrations.size(),packsEnabled?"enabled":"disabled");
    }
}

// The game paces its frames on its own retrace clock; a FIFO swapchain makes it wait
// for the display too, and the two clocks beat, so frames miss a retrace. Mailbox
// keeps vsync (no tearing) without the wait. SMS_PRESENT_FIFO=1 restores FIFO.
void sab_configure_present() {
    const char* fifo=std::getenv("SMS_PRESENT_FIFO");
    aurora::webgpu::set_prefer_mailbox(!(fifo&&*fifo&&std::strcmp(fifo,"0")!=0));
}

extern "C" {
void sab_apply_window_settings(const char* vsync,const char* mode,const char* fullscreenMode,int display,int scale) {
    WindowSettings next{true,vsync?vsync:"off",mode?mode:"windowed",fullscreenMode?fullscreenMode:"desktop",display,scale};
    if(next.vsync==windowSettings.vsync&&next.mode==windowSettings.mode&&
       next.fullscreenMode==windowSettings.fullscreenMode&&next.display==windowSettings.display&&next.scale==windowSettings.scale)return;
    windowSettings=std::move(next);
}
void sab_apply_graphics_settings(int scale,int msaa,int fxaa,int aniso,float gamma,const char* aspect,const char* filter,float wide) {
    // WebGPU exposes one or four samples for these render attachments.
    GraphicsSettings next{true,std::clamp(scale,1,8),msaa>1?4:1,fxaa!=0,std::clamp(aniso,1,16),
                      std::clamp(gamma,0.5f,2.f),wide,aspect?aspect:"keep",filter?filter:"bilinear"};
    if(next.scale==graphicsSettings.scale&&next.msaa==graphicsSettings.msaa&&next.fxaa==graphicsSettings.fxaa&&
       next.anisotropy==graphicsSettings.anisotropy&&next.gamma==graphicsSettings.gamma&&next.widescreen==graphicsSettings.widescreen&&
       next.aspect==graphicsSettings.aspect&&next.filter==graphicsSettings.filter)return;
    graphicsSettings=std::move(next);
}
void sab_delay(uint32_t ms) { SDL_Delay(ms); }
int sab_begin_menu_frame(void) {
    const int ready=sab_begin_frame();
    if(ready)aurora::gfx::retain_efb_for_ui();
    return ready;
}
void sab_set_texture_packs(int enabled,const char* const* paths,uint32_t count) {
    packsEnabled=enabled!=0;
    packPaths.clear();
    for(uint32_t i=0;i<count;++i)if(paths[i]&&*paths[i])packPaths.emplace_back(paths[i]);
    packsPending=true;
}
}
