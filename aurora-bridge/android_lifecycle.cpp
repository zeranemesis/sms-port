// Connect the actual Android Surface callbacks to Aurora's pause gate.
// Keep this JNI entry explicit in the export map: GX internals stay private.
#include "window.hpp"
#include <jni.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>

extern "C" JNIEXPORT void JNICALL
Java_org_smsunified_mobile_AuroraActivity_nativeSurfaceReady(JNIEnv*, jclass, jboolean ready) {
    aurora::window::set_surface_ready(ready == JNI_TRUE);
    // Aurora may be in SDL_WaitEvent while paused. Wake it after changing the
    // atomic gate, including when no further resize/focus event is pending.
    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        SDL_Event event{};
        event.type = SDL_EVENT_WINDOW_EXPOSED;
        SDL_PushEvent(&event);
    }
}
