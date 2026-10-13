#pragma once
#include "sms_aurora.h"
#ifdef __cplusplus
extern "C" {
#endif
SAB_API void sab_apply_window_settings(const char* vsync, const char* mode, const char* fullscreenMode,
                                      int display, int windowScale);
SAB_API void sab_apply_graphics_settings(int scale, int msaa, int fxaa, int anisotropy, float gamma,
                                        const char* aspect, const char* filter, float widescreen);
SAB_API void sab_set_overlay(int visible);
SAB_API void sab_set_texture_packs(int enabled, const char* const* paths, uint32_t count);
SAB_API void sab_delay(uint32_t milliseconds);
SAB_API int sab_begin_menu_frame(void);
SAB_API void sab_set_menu_paused(int paused);
#ifdef __cplusplus
}
#endif
