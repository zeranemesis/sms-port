#include <System/FlagManager.hpp>

extern "C" void sms_frontend_observe_game_progress(int shines, int blueCoins);

extern "C" void sms_frontend_poll_game_progress()
{
    TFlagManager* flags = TFlagManager::getInstance();
    if (flags)
        sms_frontend_observe_game_progress(flags->getFlag(0x40000),
                                           flags->getFlag(0x40001));
}

#include <System/Application.hpp>
#include <Camera/Camera.hpp>
extern "C" void sms_frontend_camera_aspect_changed(float ratio)
{
    if (gpApplication.mAppState == TApplication::APP_STATE_GAMEPLAY && gpCamera)
        gpCamera->mAspect *= ratio;
}

#include <System/MarioGamePad.hpp>
#include <GC2D/ScrnFader.hpp>
#include <JSystem/JDrama/JDRDisplay.hpp>
extern "C" int port_frame_rate;
extern "C" int port_fps60_active;
extern "C" void sms_frontend_sync_game_rate()
{
    const int previous = port_fps60_active;
    port_fps60_active = gpApplication.mAppState == TApplication::APP_STATE_GAMEPLAY && port_frame_rate == 60;
    if (gpApplication.mDisplay) gpApplication.mDisplay->unk4C = port_fps60_active ? 1 : 2;
    if (gpApplication.mFader) gpApplication.mFader->mRate = SMSGetVSyncTimesPerSec();
    if (previous == port_fps60_active) return;
    const u32 delay = (u32)(20.0f / SMSGetAnmFrameRate());
    const u32 rate = (u32)(6.0f / SMSGetAnmFrameRate());
    for (int i=0;i<4;++i) {
        TMarioGamePad* pad=gpApplication.mGamePads[i];
        if(!pad) continue;
        JUTGamePad::CButton* buttons[2]={&pad->mButton,0};
        if(pad->getPortNum()>=0) buttons[1]=&JUTGamePad::mPadButton[pad->getPortNum()];
        for(int b=0;b<2;++b) if(buttons[b]) {
            if(buttons[b]->mRepeatDelay) buttons[b]->mRepeatCount=buttons[b]->mRepeatCount*delay/buttons[b]->mRepeatDelay;
            buttons[b]->mRepeatDelay=delay;
            buttons[b]->mRepeatRate=rate;
        }
    }
}

#include <cstring>
extern "C" void sms_frontend_apply_language(const char* code)
{
    TFlagManager* flags=TFlagManager::getInstance();
    if(!flags || !code) return;
    const char* names[5]={"en","de","fr","es","it"};
    for(int i=0;i<5;++i) if(std::strcmp(code,names[i])==0) {
        // The next stage load calls the game's PAL load2DResource2Aram().
        // Do not free archives still referenced by the currently displayed HUD.
        flags->setFlag(0xA0001,i);
        break;
    }
}

extern "C" void sms_frontend_commit_disc_overlays();
void sms_frontend_commit_disc_overlays_bridge()
{
    sms_frontend_commit_disc_overlays();
}
