#pragma once
#include <SDL3/SDL.h>
// Called on the game/window thread while draining Aurora events.
bool sab_ui_process_sdl(const SDL_Event& event);
void sab_ui_overlay_draw();
void sab_ui_set_effects(int fxaa, float gamma);
void sab_apply_pending_settings();
