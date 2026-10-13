/* sms_aurora.dll: the narrow C interface between the MinGW build of the game
 * and Aurora (GX on WebGPU through Dawn), which is built with MSVC.
 *
 * The game keeps its own GX front end (platform/gx: API encoding, command
 * parser, vertex loader, physical addresses, big-endian ranges). The Aurora
 * back end (platform/gx/src/gx_aurora.cpp) turns the decoded state into a
 * little-endian command stream that Aurora's command processor executes:
 * BP/CP/XF register writes, draws with directly decoded vertices, and the
 * GX_LOAD_AURORA_* commands that carry 64-bit host pointers.
 *
 * Only plain C types cross this boundary. */
#ifndef SMS_AURORA_H
#define SMS_AURORA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SMS_AURORA_BUILD
#define SAB_API __declspec(dllexport)
#else
#define SAB_API __declspec(dllimport)
#endif

typedef struct SabConfig {
    const char* title;
    const char* userPath;   /* pipeline cache and logs; may be null */
    uint32_t windowWidth, windowHeight;
    int32_t backend;        /* AuroraBackend: 0 auto, 1 D3D11, 2 D3D12, 4 Vulkan */
    int32_t vsync;
    int32_t fullscreen;
    uint32_t msaa;
    uint32_t anisotropy;
    int32_t logLevel;       /* AuroraLogLevel: 0 debug .. 4 fatal */
} SabConfig;

/* Input and window events, already reduced to what the port uses.
 * Numbering follows SDL (the SDL2 and SDL3 values agree for all of these). */
enum {
    SAB_EV_NONE = 0,
    SAB_EV_QUIT = 0x100,
    SAB_EV_KEYDOWN = 0x300,
    SAB_EV_KEYUP = 0x301,
    SAB_EV_MOUSEMOTION = 0x400,
    SAB_EV_MOUSEBUTTONDOWN = 0x401,
    SAB_EV_PADAXIS = 0x650,
    SAB_EV_PADBUTTONDOWN = 0x651,
    SAB_EV_PADBUTTONUP = 0x652,
    SAB_EV_FOCUS_GAINED = 0x1001,
    SAB_EV_FOCUS_LOST = 0x1002,
};

typedef struct SabEvent {
    uint32_t type;
    int32_t scancode;   /* keys */
    uint16_t mod;       /* SDL keymod */
    uint8_t repeat;
    uint8_t button;     /* gamepad button / axis index */
    int32_t which;
    int32_t value;      /* axis value */
    float xrel, yrel;   /* mouse motion */
} SabEvent;

SAB_API int sab_init(const SabConfig* config);
SAB_API void sab_shutdown(void);
SAB_API const char* sab_backend_name(void);

/* Returns 1 and fills *ev while events are pending. */
SAB_API int sab_poll_event(SabEvent* ev);

/* The logical EFB (GX coordinates) the game renders to, e.g. 640x480. */
SAB_API void sab_set_efb_size(uint32_t width, uint32_t height);
SAB_API void sab_get_window_size(uint32_t* width, uint32_t* height);
SAB_API void sab_set_fullscreen(int on);
SAB_API int sab_is_fullscreen(void);
SAB_API void sab_set_vsync(int on);
SAB_API void sab_set_window_title(const char* title);
SAB_API void sab_set_mouse_relative(int on);

/* Returns 0 when the frame cannot be drawn (minimised window, lost surface);
 * commands submitted outside a frame are dropped by the caller. */
SAB_API int sab_begin_frame(void);
SAB_API void sab_end_frame(void);

/* Executes a little-endian command stream immediately. */
SAB_API void sab_process(const void* data, uint32_t size);

/* EFB -> texture copy into a GPU texture keyed by dest; a later texture whose
 * data pointer is dest samples it. fmt is the GXTexFmt of the copy. */
SAB_API void sab_copy_tex(void* dest, uint16_t left, uint16_t top, uint16_t width, uint16_t height,
                          uint16_t dstWidth, uint16_t dstHeight, uint32_t fmt, int clear);
SAB_API void sab_destroy_copy_tex(void* dest);

SAB_API void sab_set_window_icon(const uint8_t* rgba, int width, int height);

#ifdef __cplusplus
}
#endif

#endif
