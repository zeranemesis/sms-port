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
SAB_API int sab_begin_data_pass(uint16_t width, uint16_t height);
SAB_API void sab_end_data_pass(void);
SAB_API uint32_t sab_copy_readback_capacity(void);
SAB_API int sab_flush_copy_readbacks(void);
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
/* Submit game work without necessarily presenting it. VI selects scanout. */
SAB_API void sab_finish_frame(int present);
/* The next presented frame is shown no earlier than this long after it ends
 * (frame generation: a generated image half a game frame after the game's). */
SAB_API void sab_present_after(uint32_t microseconds);

/* Frame generation: a frame made of recorded operations (the game's stream
 * with its matrices moved on, its copies), recorded and presented on the GX
 * thread without the caller waiting, no earlier than presentAfterUs from now.
 * The stream bytes are not copied: they must stay valid until the next call
 * that waits for the GX thread (any other sab_* call but sab_process). */
enum {
    SAB_REPLAY_STREAM = 0,
    SAB_REPLAY_COPY_TEX = 1,
    SAB_REPLAY_COPY_DISPLAY = 2,
    SAB_REPLAY_DATA_PASS_BEGIN = 3,
    SAB_REPLAY_DATA_PASS_END = 4,
};
typedef struct SabReplayOp {
    int kind;
    const void* bytes; /* SAB_REPLAY_STREAM: the command stream */
    uint32_t size;
    void* dest;
    uint16_t x, y, w, h, dw, dh;
    uint32_t fmt;
    int clear;
} SabReplayOp;
SAB_API void sab_submit_replay(const SabReplayOp* ops, uint32_t count, const void* xfb, uint32_t presentAfterUs);
/* sab_begin_frame (and sab_retain_efb) run in order on the GX thread, without
 * waiting: the next frame's stream can be queued behind a replay. */
SAB_API void sab_begin_frame_async(int retainEfb);
/* Statistics since the last call: the GX thread's working time, and the time
 * callers waited for it. */
SAB_API void sab_gx_thread_times(double* busySeconds, double* waitedSeconds);
/* Statistics since the last call (5 values, seconds): frame generation's replay
 * begin, stream, copies, end, and the asynchronous begins. */
SAB_API void sab_gx_thread_breakdown(double* seconds);
SAB_API void sab_retain_efb(void);
SAB_API int sab_copy_display(const void* dest, uint16_t left, uint16_t top,
                             uint16_t width, uint16_t height, int clear);
SAB_API int sab_select_scanout(const void* dest, int black);

/* Executes a little-endian command stream immediately. */
SAB_API void sab_process(const void* data, uint32_t size);

/* Synchronous EFB checkpoint, cached until revision/frame changes. Returns 0
 * outside a frame or for invalid coordinates. Colour is GX ARGB, depth Z24. */
SAB_API int sab_peek_efb(uint32_t x, uint32_t y, uint64_t revision, uint32_t* argb, uint32_t* z);
/* Exact current-pass depth -> CPU RAM, complete before return. GX Z24X8 bytes
 * are tiled 4x4, big-endian X/Zhi and Zmid/Zlo planes. On failure leaves dest
 * unchanged. This CPU height-field path is separate from async colour copies. */
SAB_API int sab_copy_depth_z24x8_sync(void* dest, uint16_t left, uint16_t top,
                                     uint16_t width, uint16_t height,
                                     uint16_t dstWidth, uint16_t dstHeight);
/* The same exact depth copy without waiting for the GPU. Returns 0 when it is
 * not queued (another is still in flight, or invalid arguments). tag must be
 * non-zero and is handed back to sab_take_depth_z24x8. */
SAB_API int sab_copy_depth_z24x8_async(uint16_t left, uint16_t top, uint16_t width,
                                      uint16_t height, uint64_t tag);
/* Collects a finished asynchronous depth copy. 1: it was the copy for tag and
 * dest now holds its Z24X8 bytes (same layout as the synchronous copy). 0: not
 * finished yet. -1: finished but failed, for another tag, or dest was null;
 * dest is left unchanged. width/height are the source rectangle's size. */
SAB_API int sab_take_depth_z24x8(uint64_t tag, void* dest, uint16_t width, uint16_t height,
                                uint16_t dstWidth, uint16_t dstHeight);

/* EFB -> texture copy into a GPU texture keyed by dest; a later texture whose
 * data pointer is dest samples it. fmt is the GXTexFmt of the copy. With
 * readback, the copied EFB pixels (dstWidth x dstHeight, RGBA8, unconverted)
 * also come back through sab_poll_copy_readback a frame or two later. */
SAB_API void sab_copy_tex(void* dest, uint16_t left, uint16_t top, uint16_t width, uint16_t height,
                          uint16_t dstWidth, uint16_t dstHeight, uint32_t fmt, int clear, int readback);
SAB_API int sab_poll_copy_readback(const void** dest, uint32_t* width, uint32_t* height, uint64_t* frameId,
                                   const uint32_t** rgba);
/* Identified colour copies while the backend is active: each readback request
 * produces a success or failure result (shutdown cancels remaining requests).
 * A failed result has success == 0 and rgba == NULL. IDs must be nonzero. */
SAB_API void sab_copy_tex_request(void* dest, uint16_t left, uint16_t top, uint16_t width, uint16_t height,
                                  uint16_t dstWidth, uint16_t dstHeight, uint32_t fmt, int clear,
                                  int readback, uint64_t requestId);
SAB_API int sab_poll_copy_readback_request(const void** dest, uint32_t* width, uint32_t* height,
                                           uint64_t* frameId, const uint32_t** rgba,
                                           uint64_t* requestId, int* success);

/* Pixel metrics: the GX_LOAD_AURORA_PIXEL_METRIC command (0x0100, then a u8
 * slot + 1, or 0 to stop) in the command stream selects the counter. Counts
 * come back per frame (256 slots), in EFB pixels: divide by
 * sab_efb_pixel_area_scale() for GX pixels. */
SAB_API uint64_t sab_frame_id(void);
SAB_API float sab_efb_pixel_area_scale(void);
SAB_API int sab_poll_pixel_metrics(uint64_t* frameId, const uint32_t** counts);
SAB_API void sab_destroy_copy_tex(void* dest);

SAB_API void sab_set_window_icon(const uint8_t* rgba, int width, int height);

#ifdef __cplusplus
}
#endif

#endif
