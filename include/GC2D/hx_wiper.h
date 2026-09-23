#ifndef GC2D_HX_WIPER_H
#define GC2D_HX_WIPER_H

#include <dolphin/types.h>

#if __cplusplus
extern "C" {
#endif

// NOTE: only the global (non-static) functions of hx_wiper.c are declared
// here, everything else is local to the translation unit.

int Hx_MovieStartSyncEx(void);
void Hx_MovieStartSync(void);
u8 Hx_UpdateWipe(f32 delta);
u8 Hx_GetWipeType(int type);
void Hx_StartWipe(u8 type, int param);
void Hx_RemoveResource(void);
void Hx_ProvideResourceEx(void* resource);
void Hx_ProvideResource(void* resource, u32 size);
void Hx_ResetWipe(u32 width, u32 height);

#if __cplusplus
}
#endif

#endif
