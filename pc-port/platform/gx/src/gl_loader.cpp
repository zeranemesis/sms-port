#include "gl_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "gx_internal.h"

namespace gx { namespace gl {
#define SMS_GX_DEFINE(type, name) type gx_##name = nullptr;
SMS_GX_GL_FUNCS(SMS_GX_DEFINE)
#undef SMS_GX_DEFINE

// SMS_GX_STATS counts the GL calls the renderer makes: each entry point is
// wrapped in a trampoline that bumps g_statGlCalls (not on 32-bit Windows,
// whose GL entry points are __stdcall).
uint64_t g_statGlCalls = 0;
#if !(defined(_WIN32) && !defined(_WIN64))
template <typename F> struct Counted;
template <typename R, typename... A> struct Counted<R (*)(A...)> {
    template <R (**Real)(A...), uint64_t* N> static R call(A... a) {
        g_statGlCalls++;
        (*N)++;
        return (*Real)(a...);
    }
};
struct CallCount {
    const char* name;
    uint64_t* n;
};
static CallCount s_counts[256];
static int s_nCounts = 0;
#endif

// Logs the entry points called most since the previous call (SMS_GX_STATS).
void logTopCalls(uint32_t frames) {
#if !(defined(_WIN32) && !defined(_WIN64))
    static uint64_t last[256];
    int order[256];
    for (int i = 0; i < s_nCounts; i++) order[i] = i;
    auto delta = [&](int i) { return *s_counts[i].n - last[i]; };
    for (int i = 1; i < s_nCounts; i++)  // insertion sort, descending
        for (int j = i; j > 0 && delta(order[j]) > delta(order[j - 1]); j--) {
            int t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    char line[1024];
    int len = 0;
    for (int k = 0; k < 12 && k < s_nCounts && delta(order[k]); k++)
        len += snprintf(line + len, sizeof line - size_t(len), "%s%s %.0f", k ? ", " : "", s_counts[order[k]].name,
                        double(delta(order[k])) / frames);
    if (len) gx::logmsg("stats: GL calls/frame by entry point: %s", line);
    for (int i = 0; i < s_nCounts; i++) last[i] = *s_counts[i].n;
#else
    (void)frames;
#endif
}

bool load(void* (*getProc)(const char*)) {
    bool ok = true;
#define SMS_GX_LOAD(type, name)                                   \
    gx_##name = reinterpret_cast<type>(getProc(#name));           \
    if (!gx_##name) { gx::logmsg("GL entry point missing: %s", #name); ok = false; }
    SMS_GX_GL_FUNCS(SMS_GX_LOAD)
#undef SMS_GX_LOAD
#if !(defined(_WIN32) && !defined(_WIN64))
    if (getenv("SMS_GX_STATS")) {
#define SMS_GX_WRAP(type, name)                                   \
        {                                                         \
            static type real_##name;                              \
            static uint64_t n_##name;                             \
            real_##name = gx_##name;                              \
            gx_##name = &Counted<type>::template call<&real_##name, &n_##name>; \
            if (s_nCounts < 256) s_counts[s_nCounts++] = {#name, &n_##name}; \
        }
        SMS_GX_GL_FUNCS(SMS_GX_WRAP)
#undef SMS_GX_WRAP
    }
#endif
    return ok;
}
}}  // namespace gx::gl
