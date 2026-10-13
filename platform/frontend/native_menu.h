#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void GXPC_MenuInit(void* window, void* rendererContext);
int GXPC_MenuVisible(void);
void GXPC_MenuToggle(void);
int GXPC_MenuProcessEvent(const void* event);
void GXPC_MenuDraw(int width, int height);
void GXPC_MenuShutdown(void);
#ifdef __cplusplus
}
#endif
