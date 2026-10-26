/* Optional SDL3 host audio, independent of Aurora renderer initialization. */
#ifndef SMS_AURORA_AUDIO_H
#define SMS_AURORA_AUDIO_H

#include "sms_aurora.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SabAudioOutput* SabAudioHandle;
/* Fill all bytes with host-endian signed 16-bit stereo PCM, left then right,
 * at 32000 Hz. len is positive, a multiple of four, and at most 2048 bytes.
 * Called on an SDL audio thread; must not throw or call audio lifecycle APIs. */
typedef void (*SabAudioCallback)(void* userdata, uint8_t* pcm, int len);

/* Returns a paused output, or NULL. Does not require sab_init/s_ready.
 * The provider/userdata must remain valid until close returns. */
SAB_API SabAudioHandle sab_audio_open(SabAudioCallback callback, void* userdata);
/* Publish the AI device-mode state before starting: callbacks may run as soon
 * as this succeeds. Returns 1 on success, 0 on failure. */
SAB_API int sab_audio_start(SabAudioHandle handle);
/* Control-thread operations only, never from the provider callback. Neither
 * close operation may be called with the AI FIFO mutex held. They wait for
 * in-flight callbacks; close_all must precede SDL_Quit/Aurora shutdown.
 * A closed handle must not be reused. NULL is accepted by close. */
SAB_API void sab_audio_close(SabAudioHandle handle);
SAB_API void sab_audio_close_all(void);

#ifdef __cplusplus
}
#endif
#endif
