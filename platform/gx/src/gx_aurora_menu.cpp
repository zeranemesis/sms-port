#include "sms_gx/gx_pc.h"
#include "gx_live_settings.h"
#include "sms_aurora_ui.h"
#include "sms_aurora_settings.h"
#include "../../frontend/partyboard_backend.h"
#include "../../frontend/partyboard_menu.h"
#include "../../frontend/native_menu.h"
#include "../../frontend/settings_store.h"
#include "../../frontend/retroachievements.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>

extern "C" {
void sms_frontend_audio_volume(int);
void sms_frontend_audio_menu_pause(int);
void sms_frontend_reload_input();
float sms_frontend_apply_game_settings(const char*,int,int);
void sms_frontend_sync_game_rate();
void sms_frontend_apply_language(const char*);
#ifdef __ANDROID__
void sms_mobile_set_menu_visible(int);
#endif
}
namespace {
using namespace sms_frontend::store;
SettingsFile settings;
BindingsFile bindings;
std::unique_ptr<sms_frontend::PartyBoardMenu> menu;
bool visible = false;

bool enabled(const std::string& s) { return s=="on"||s=="1"||s=="yes"||s=="true"; }
std::string utf8(const std::filesystem::path& path) {
    const auto bytes=path.generic_u8string();
    return {reinterpret_cast<const char*>(bytes.data()),bytes.size()};
}
void environment(const char* key,const std::string& value) {
#ifdef _WIN32
    _putenv_s(key,value.c_str());
    SetEnvironmentVariableA(key,value.c_str());
#else
    setenv(key,value.c_str(),1);
#endif
}
std::vector<std::string> mods() {
    std::error_code ec;
    auto base=std::filesystem::u8path(settings.path).parent_path()/"mods";
    std::vector<std::string> result;
    for(auto it=std::filesystem::directory_iterator(base,ec);!ec&&it!=std::filesystem::directory_iterator();it.increment(ec)) {
        if(it->is_directory(ec)) {
            const std::string name=utf8(it->path().filename());
            if(name.empty()||name[0]=='.'||name=="textures"||name=="hd-cutscenes")continue;
            std::error_code entryError;
            if(std::filesystem::is_directory(it->path()/"files",entryError)) result.push_back(name);
        }
    }
    std::sort(result.begin(),result.end());return result;
}
void applySettings() {
#ifndef __ANDROID__
    GXPC_ApplyMenuWindowSettings(settings.get("vsync","off").c_str(),settings.get("window_mode","windowed").c_str(),
        settings.get("fullscreen_mode","desktop").c_str(),std::atoi(settings.get("display","0").c_str()),
        std::atoi(settings.get("window_scale","0").c_str()),enabled(settings.get("mouse_camera","off")));
#endif
    static const char* keys[]={"camera_invert_x","camera_invert_y","camera_speed","free_camera","mouse_sensitivity"};
    static const char* variables[]={"SMS_CAMERA_INVERT_X","SMS_CAMERA_INVERT_Y","SMS_CAMERA_SPEED","SMS_FREE_CAMERA","SMS_MOUSE_SENSITIVITY"};
    static const char* defaults[]={"off","off","100","off","100"};
    for(int i=0;i<5;++i) environment(variables[i],settings.get(keys[i],defaults[i]));
    environment("SMS_BINDINGS",bindings.path);
    sms_frontend_audio_volume(enabled(settings.get("audio","on"))?std::atoi(settings.get("volume","100").c_str()):0);
    sms_frontend_reload_input();
    std::string wideSetting=settings.get("widescreen","off");
#ifdef __ANDROID__
    if(wideSetting=="auto") { uint32_t w=0,h=0;sab_get_window_size(&w,&h);if(h)wideSetting=std::to_string(float(w)/float(h)); }
#endif
    const float wide=sms_frontend_apply_game_settings(wideSetting.c_str(),
        std::atoi(settings.get("frame_rate","30").c_str()),enabled(settings.get("skip_movies","off")));
    sms_frontend_sync_game_rate();
    sms_frontend_apply_language(settings.get("language","en").c_str());
    GXPC_ApplyMenuGraphicsSettings(std::atoi(settings.get("resolution","1").c_str()),std::atoi(settings.get("msaa","0").c_str()),
        enabled(settings.get("fxaa","off")),std::atoi(settings.get("anisotropic","0").c_str()),
        std::atof(settings.get("brightness","1.0").c_str()),settings.get("aspect","keep").c_str(),
        settings.get("present_filter","bilinear").c_str(),wide,settings.get("widescreen_hud","centre")=="edges");
    environment("SMS_MOD",settings.get("mod","none"));
    environment("SMS_HD_CUTSCENES",enabled(settings.get("hd_cutscenes","on"))?"":"0");
    GXPC_ApplyMenuTexturePacks(enabled(settings.get("texture_packs","on")));
    static std::string selectedMod;
    if(selectedMod!=settings.get("mod","none")) { selectedMod=settings.get("mod","none");GXPC_RefreshMenuTexturePacks(); }
    if(GXPC_OverlayVisible()!=enabled(settings.get("overlay","off"))) GXPC_OverlayToggle();
}
bool save() {
    if(!(settings.save()&bindings.save()))return false;
    applySettings();return true;
}
void resume() {
#ifdef __ANDROID__
    sms_mobile_set_menu_visible(0);
#endif
    visible=false;sab_set_menu_paused(0);sms_frontend_audio_menu_pause(0);sms_frontend_reload_input();sms_partyboard::ResetInput();
}
}

extern "C" {
void GXPC_MenuInit(void*,void*) {
    if(menu)return;
    settings.path=getenv("SMS_SETTINGS")?getenv("SMS_SETTINGS"):"settings.txt";
    bindings.path=getenv("SMS_BINDINGS")?getenv("SMS_BINDINGS"):"bindings.txt";
    settings.load();bindings.load();
#ifdef __ANDROID__
    if(settings.values.find("widescreen")==settings.values.end())settings.set("widescreen","auto");
    if(settings.values.find("resolution")==settings.values.end())settings.set("resolution","0");
    if(settings.values.find("vsync")==settings.values.end())settings.set("vsync","on");
#endif
    static const char* keys[]={"vsync","window_mode","fullscreen_mode","display","window_scale","mouse_camera","volume","audio","resolution","frame_rate","widescreen","aspect","overlay","language","skip_movies"};
    static const char* variables[]={"SMS_VSYNC","SMS_WINDOW_MODE","SMS_FULLSCREEN_MODE","SMS_DISPLAY","SMS_WINDOW_SCALE","SMS_MOUSE_CAMERA","SMS_VOLUME","SMS_AUDIO","SMS_GX_SCALE","SMS_FRAME_RATE","SMS_WIDESCREEN","SMS_ASPECT","SMS_OVERLAY","SMS_LANGUAGE","SMS_SKIP_MOVIES"};
    for(size_t i=0;i<sizeof(keys)/sizeof(keys[0]);++i)
        if(const char* value=getenv(variables[i]))settings.set(keys[i],value);
    for(const char* key:{"audio","mouse_camera","overlay","skip_movies","camera_invert_x","camera_invert_y","free_camera"})
        settings.set(key,enabled(settings.get(key,!std::strcmp(key,"audio")?"on":"off"))?"on":"off");
    if(settings.get("vsync","off")=="1")settings.set("vsync","on");
    if(settings.get("vsync","off")=="0")settings.set("vsync","off");
    if(settings.get("widescreen","off")=="0")settings.set("widescreen","off");
    static const char* graphicsKeys[]={"msaa","fxaa","anisotropic","brightness","present_filter","widescreen_hud","camera_invert_x","camera_invert_y","camera_speed","free_camera","mouse_sensitivity"};
    static const char* graphicsVariables[]={"SMS_MSAA","SMS_FXAA","SMS_ANISO","SMS_GAMMA","SMS_PRESENT_FILTER","SMS_WIDESCREEN_HUD","SMS_CAMERA_INVERT_X","SMS_CAMERA_INVERT_Y","SMS_CAMERA_SPEED","SMS_FREE_CAMERA","SMS_MOUSE_SENSITIVITY"};
    for(size_t i=0;i<sizeof(graphicsKeys)/sizeof(graphicsKeys[0]);++i)
        if(const char* value=getenv(graphicsVariables[i]))settings.set(graphicsKeys[i],value);
    for(const char* key:{"fxaa","camera_invert_x","camera_invert_y","free_camera"})
        settings.set(key,enabled(settings.get(key,"off"))?"on":"off");
    if(const char* value=getenv("SMS_TEXTURE_PACKS"))settings.set("texture_packs",std::strcmp(value,"0")&&std::strcmp(value,"off")?"on":"off");
    if(const char* value=getenv("SMS_HD_CUTSCENES"))settings.set("hd_cutscenes",std::strcmp(value,"0")&&std::strcmp(value,"off")?"on":"off");
    if(const char* value=getenv("SMS_MOD"))settings.set("mod",value);
    if(std::atoi(settings.get("msaa","0").c_str())>1)settings.set("msaa","4");
    uint32_t width=0,height=0;sab_get_window_size(&width,&height);
    if(!sms_partyboard::Initialize(nullptr,nullptr,int(width),int(height))) {
        std::fprintf(stderr,"[partyboard] Aurora menu initialization failed\n");return;
    }
    sms_frontend::ra::initialize();
    const char* disc=getenv("SMS_DISC_IMAGE");
    if(!disc||!*disc)disc=getenv("SMS_DISC_ROOT");
    if(disc&&*disc)sms_frontend::ra::set_disc_path(disc);
    sms_frontend::MenuBindings actions;
    actions.auroraRenderer=true;
    actions.getSetting=[](const char* key,const char* fallback){return settings.get(key,fallback);};
    actions.setSetting=[](const char* key,const std::string& value){settings.set(key,value);};
    actions.getBinding=[](int i){return i>=0&&i<kNumControls?bindings.keys[i]:std::string();};
    actions.setBinding=[](int i,const std::string& key){if(i>=0&&i<kNumControls){bindings.keys[i]=key;save();}};
    actions.bindingLabels.assign(kControlLabels,kControlLabels+kNumControls);
    actions.availableMods=mods();actions.refreshMods=mods;
    actions.onModsInstalled=[]{GXPC_RefreshMenuTexturePacks();};
    actions.onSave=save;
    actions.onResume=[]{if(!save())return false;resume();return true;};
    actions.onQuit=[]{sab_ui_request_quit();};
    const auto resources=std::filesystem::u8path(settings.path).parent_path()/"res"/"partyboard";
    if(std::filesystem::is_directory(resources))actions.resourceDirectory=utf8(resources);
    menu=std::make_unique<sms_frontend::PartyBoardMenu>(sms_partyboard::GetContext(),actions);
    if(!menu->available()) {
        std::fprintf(stderr,"[partyboard] menu resources could not be loaded\n");
        GXPC_MenuShutdown();return;
    }
    std::fprintf(stderr,"[partyboard] Sunshine menu ready on Aurora (F1 / controller Guide)\n");
    applySettings();
    if(enabled(settings.get("menu_start","off"))||(getenv("SMS_MENU_ON_START")&&enabled(getenv("SMS_MENU_ON_START"))))GXPC_MenuToggle();
}
int GXPC_MenuVisible(void) { return menu&&visible; }
void GXPC_MenuToggle(void) {
    if(!menu)return;
    if(visible) { if(!save())return;resume();menu->hide(); }
    else {
#ifndef __ANDROID__
        const bool fullscreen=sab_is_fullscreen()!=0;
        if(!fullscreen)settings.set("window_mode","windowed");
        else if(settings.get("window_mode","windowed")=="windowed")settings.set("window_mode","borderless");
#endif
        bindings.load();visible=true;
#ifdef __ANDROID__
        sms_mobile_set_menu_visible(1);
#endif
        sab_set_menu_paused(1);sms_frontend_audio_menu_pause(1);sms_frontend_reload_input();sms_partyboard::ResetInput();menu->show();
    }
}
int GXPC_MenuProcessEvent(const void* raw) {
    if(!raw)return 0;
    const auto& e=*static_cast<const SabUiEvent*>(raw);
    if(((e.type==SAB_UI_KEY_DOWN||e.type==SAB_UI_KEY_UP)&&e.scancode==58)||
       ((e.type==SAB_UI_PAD_DOWN||e.type==SAB_UI_PAD_UP)&&e.button==5)) {
        if((e.type==SAB_UI_KEY_DOWN&&!e.repeat)||e.type==SAB_UI_PAD_DOWN)GXPC_MenuToggle();
        return 1;
    }
    if(!visible||!menu)return 0;
    if(menu->waitingForBinding()&&e.type==SAB_UI_KEY_DOWN&&!e.repeat)menu->captureBinding(keyName(e.scancode));
    else sms_partyboard::ProcessEvent(raw);
    return 1;
}
void GXPC_MenuDraw(int width,int height) {
    sms_frontend::ra::pump();
    if(visible&&menu){menu->update();sms_partyboard::Render(width,height);}
}
void GXPC_MenuShutdown(void) {
    if(!menu&&!sms_partyboard::GetContext())return;
    visible=false;sms_frontend_audio_menu_pause(0);menu.reset();
    sms_frontend::ra::shutdown();sms_partyboard::Shutdown();
}
}
