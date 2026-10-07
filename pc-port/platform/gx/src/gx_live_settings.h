#ifndef SMS_GX_LIVE_SETTINGS_H
#define SMS_GX_LIVE_SETTINGS_H

// Frontend-only runtime settings. Call on the window's GL thread after saving
// the menu. Negative display/windowScale values preserve the current setting.
#ifdef __cplusplus
extern "C" {
#endif
void GXPC_ApplyMenuWindowSettings(const char* vsync, const char* windowMode,
                                const char* fullscreenMode, int display,
                                int windowScale, int mouseCamera);
void GXPC_ApplyMenuTexturePacks(int enabled);
void GXPC_RefreshMenuTexturePacks();
float GXPC_GetMenuWidescreen();
void GXPC_ApplyMenuGraphicsSettings(int scale, int msaa, int fxaa, int aniso,
                                    float gamma, const char* aspect, const char* filter,
                                    float widescreen, int hudEdges);
#ifdef __cplusplus
}
#endif
#endif
