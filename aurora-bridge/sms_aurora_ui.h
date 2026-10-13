#ifndef SMS_AURORA_UI_H
#define SMS_AURORA_UI_H
#include "sms_aurora.h"

/* UI objects remain in their compiler's runtime. Only C data and opaque
 * integer handles cross the MSVC/MinGW boundary. Callback data is borrowed
 * for the duration of the call; it must not be retained. */
#ifdef __cplusplus
extern "C" {
#endif

enum { SAB_UI_ABI_VERSION = 1 };
enum {
    SAB_UI_KEY_DOWN = 1, SAB_UI_KEY_UP, SAB_UI_MOUSE_MOVE,
    SAB_UI_MOUSE_DOWN, SAB_UI_MOUSE_UP, SAB_UI_MOUSE_WHEEL,
    SAB_UI_TEXT, SAB_UI_PAD_AXIS, SAB_UI_PAD_DOWN, SAB_UI_PAD_UP,
    SAB_UI_FOCUS_LOST, SAB_UI_MOUSE_LEAVE
};
typedef struct SabUiEvent {
    uint32_t type, keycode;
    int32_t scancode, which, value;
    uint16_t modifiers;
    uint8_t repeat, button;
    float x, y;
    const char* text;
} SabUiEvent;
typedef struct SabUiCallbacks {
    uint32_t abiVersion;
    void* userdata;
    void (*draw)(void* userdata, int width, int height, float density);
    int (*event)(void* userdata, const SabUiEvent* event);
} SabUiCallbacks;

typedef struct SabUiVertex {
    float x, y;
    uint8_t color[4];
    float u, v;
} SabUiVertex;
typedef struct SabUiColorStop {
    uint8_t color[4];
    float position;
    uint32_t unit;
} SabUiColorStop;
enum { SAB_UI_NUMBER = 1, SAB_UI_BOOLEAN, SAB_UI_VECTOR2, SAB_UI_COLOR, SAB_UI_STOPS };
typedef struct SabUiParameter {
    const char* name;
    uint32_t type;
    float value[4];
    const SabUiColorStop* stops;
    uint32_t stopCount;
} SabUiParameter;

SAB_API int sab_ui_initialize(const char* rmlVersion, const SabUiCallbacks* callbacks);
SAB_API void sab_ui_shutdown(void);
SAB_API double sab_ui_time(void);
SAB_API void sab_ui_cursor(const char* name);
SAB_API void sab_ui_clipboard_set(const char* text);
/* Returns the byte count including the trailing NUL. */
SAB_API uint32_t sab_ui_clipboard_get(char* text, uint32_t capacity);
SAB_API void sab_ui_keyboard(int enabled, float x, float y, float lineHeight);
SAB_API void sab_ui_request_quit(void);

SAB_API uint64_t sab_ui_geometry_create(const SabUiVertex* vertices, uint32_t vertexCount,
                                       const int32_t* indices, uint32_t indexCount);
SAB_API void sab_ui_geometry_draw(uint64_t geometry, float x, float y, uint64_t texture);
SAB_API void sab_ui_geometry_release(uint64_t geometry);
SAB_API uint64_t sab_ui_texture_create(const uint8_t* rgba, int width, int height);
SAB_API void sab_ui_texture_release(uint64_t texture);
SAB_API void sab_ui_scissor_enable(int enabled);
SAB_API void sab_ui_scissor_set(int left, int top, int right, int bottom);
SAB_API void sab_ui_clip_enable(int enabled);
SAB_API void sab_ui_clip_draw(int operation, uint64_t geometry, float x, float y);
/* Row-major logical matrix values; NULL selects identity. */
SAB_API void sab_ui_transform(const float* rows16);
SAB_API uint64_t sab_ui_layer_push(void);
SAB_API void sab_ui_layer_composite(uint64_t source, uint64_t destination, int replace,
                                   const uint64_t* filters, uint32_t count);
SAB_API void sab_ui_layer_pop(void);
SAB_API uint64_t sab_ui_layer_texture(void);
SAB_API uint64_t sab_ui_layer_mask(void);
SAB_API uint64_t sab_ui_filter_create(const char* name, const SabUiParameter* parameters, uint32_t count);
SAB_API void sab_ui_filter_release(uint64_t filter);
SAB_API uint64_t sab_ui_shader_create(const char* name, const SabUiParameter* parameters, uint32_t count);
SAB_API void sab_ui_shader_draw(uint64_t shader, uint64_t geometry, float x, float y, uint64_t texture);
SAB_API void sab_ui_shader_release(uint64_t shader);

#ifdef __cplusplus
}
#endif
#endif
