#ifndef SMS_PARTYBOARD_BACKEND_H
#define SMS_PARTYBOARD_BACKEND_H

namespace Rml { class Context; }

namespace sms_partyboard {
// The game's existing SDL window and GL 3.3 context stay owned by GX.
// All calls, including Shutdown, must run on that context's thread.
bool Initialize(void* window, void* glContext, int drawableWidth, int drawableHeight);
Rml::Context* GetContext();
// Returns true when RmlUi consumes the SDL2 event. The menu's caller should
// independently block all game input while the menu is visible.
bool ProcessEvent(const void* sdlEvent);
// Call on opening/closing the menu so a held/released controller stick cannot
// carry stale navigation state across the pause transition.
void ResetInput();
// Updates the context, renders layers/filters, and restores the game's GL state.
void Render(int drawableWidth, int drawableHeight);
void Shutdown();
}
#endif
