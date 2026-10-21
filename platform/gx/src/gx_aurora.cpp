// Aurora back end: renders through sms_aurora.dll (Aurora, GX on WebGPU).
//
// The front end is unchanged: gx_api.cpp encodes the API into register writes
// and gx_fifo.cpp parses every command stream into the register file and
// decodes vertices. This file replaces the OpenGL renderer and the SDL2
// window (gx_render, gx_shader, gx_texture, gx_platform). It forwards the
// register writes Aurora understands as a little-endian command stream, owns
// vertex attribute format 0 (every attribute direct, already decoded to
// floats), and turns the texture state into GX_LOAD_AURORA_TEXOBJ/TLUT
// commands that carry host pointers, since Aurora never follows the 32-bit
// physical addresses of BP texture, TLUT and copy registers.
#include "gx_internal.h"
#include "sms_gx/gx_pc.h"
#include "sms_aurora.h"
#include "sms_aurora_settings.h"
#include "gx_live_settings.h"
#ifdef SMS_PARTYBOARD_UI
#include "../../frontend/native_menu.h"
#endif

#include <dolphin/gx.h>

#include <direct.h>
#include <string>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <string>
#include <map>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tuple>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" uint32_t VIGetRetraceCount(void);
extern "C" int port_fps60_active __attribute__((weak));  // the game's (frame rate patches)
extern "C" int port_frame_rate __attribute__((weak));            // port_runtime.cpp
extern "C" int port_frame_rate_auto_capped __attribute__((weak));
extern "C" void sms_frontend_sync_game_rate(void) __attribute__((weak));
extern "C" double port_vi_take_late_max(void) __attribute__((weak));  // vi.cpp
extern "C" void port_os_profile_frame(unsigned) __attribute__((weak));

namespace gx {

bool g_gxStats = false;
double g_flushSeconds = 0;
void (*g_displayCopyHook)(const void* xfb) = nullptr;
extern uint32_t g_texCopyFmt;  // gx_api.cpp: the GXTexFmt of the last GXSetTexCopyDst

namespace {

bool s_ready = false;
bool s_inFrame = false;
bool s_dataPassActive = false;
uint32_t s_frame = 0;
uint32_t s_efbW = 0, s_efbH = 0;
float s_widescreen = 1.f;
bool s_hudEdges=false,s_hud=false,s_stretch2D=false;
std::vector<std::pair<float,float>> s_hudPanes;
bool s_texturePacks=true;

std::vector<std::string> texturePackPaths() {
    std::vector<std::string> paths;
    const char* e=getenv("SMS_TEXTURE_PACKS");
    if(e&&*e&&strcmp(e,"0")&&strcmp(e,"1")&&strcmp(e,"on")) {
        std::string current;
        for(char c:std::string(e)+";") {
            if(c==';'||(c==':'&&current.size()!=1)) { if(!current.empty())paths.push_back(current);current.clear(); }
            else current+=c;
        }
    } else paths={"mods/textures","../../mods/textures"};
    if(const char* selected=getenv("SMS_MOD")) {
        std::string name;
        for(char c:std::string(selected)+";") {
            if(c==';'||c==',') {
                if(!name.empty()&&name!="none"&&name!="0") {
                    paths.push_back("../../mods/"+name+"/textures");
                    paths.push_back("mods/"+name+"/textures");
                    paths.push_back(name+"/textures");
                }
                name.clear();
            } else name+=c;
        }
    }
    return paths;
}
void refreshTexturePacks() {
    const auto paths=texturePackPaths();
    std::vector<const char*> raw;
    for(const auto& path:paths)raw.push_back(path.c_str());
    sab_set_texture_packs(s_texturePacks,raw.data(),uint32_t(raw.size()));
}
bool s_mouseCamera = false, s_mouseCaptured = false, s_mouseReleased = false;
bool s_hasFocus = true;
bool s_overlay = false;
GXPCVIScanoutFn s_viScanout = nullptr;
bool s_autoPresent = true, s_haveScanout = false, s_pollingVI = false;
std::thread::id s_graphicsThread;
std::unordered_map<const void*, uint64_t> s_xfbSerials;
const void* s_lastCopiedXfb = nullptr;
const void* s_presentedXfb = nullptr;
uint64_t s_copySerial = 0, s_presentedSerial = 0, s_presentedCount = 0;
bool s_presentedBlack = true;
uint32_t s_presentedWidth = 0, s_presentedHeight = 0;
void (*s_eventCb)(const union SDL_Event*) = nullptr;
double (*s_idleClock)(void) = nullptr;

double nowSeconds() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return double(t.tv_sec) + double(t.tv_nsec) * 1e-9;
}

// ------------------------------------------------------------------ command stream
std::vector<uint8_t> s_cmd;

inline void put8(uint8_t v) { s_cmd.push_back(v); }
inline void put16(uint16_t v) {
    size_t n = s_cmd.size();
    s_cmd.resize(n + 2);
    memcpy(&s_cmd[n], &v, 2);
}
inline void put32(uint32_t v) {
    size_t n = s_cmd.size();
    s_cmd.resize(n + 4);
    memcpy(&s_cmd[n], &v, 4);
}
inline void put64(uint64_t v) {
    size_t n = s_cmd.size();
    s_cmd.resize(n + 8);
    memcpy(&s_cmd[n], &v, 8);
}
inline void putBP(uint32_t reg, uint32_t data) {
    put8(0x61);
    put32(reg << 24 | (data & 0xFFFFFF));
}
inline void putCP(uint8_t reg, uint32_t value) {
    put8(0x08);
    put8(reg);
    put32(value);
}
inline void putAurora(uint16_t sub) {
    put8(0x50);  // GX_LOAD_AURORA
    put16(sub);
}

// Frame generation (SMS_FRAMEGEN=1, a prototype): each frame's command stream
// is kept, and right after the game's frame is presented it is played again
// with every matched object's matrices carried half a game frame further along
// their motion, and that image is presented half a frame later: 120 images a
// second from the game's 60. See the section "frame generation" below.
bool fgEnabled() {
    static const bool enabled = [] { const char* v = getenv("SMS_FRAMEGEN"); return v && *v && strcmp(v, "0"); }();
    return enabled;
}
void fgTakeStream();
void fgNoteMetric(size_t slotByte);
void fgNoteDraw(uint8_t opcode, uint32_t count, uint32_t fmt, const VtxFmtLayout& L);

double s_submitSeconds = 0;  // statistics: Aurora's command processing on this thread
double s_submitTotalSeconds = 0; // Monotonic diagnostic counter, independent of report resets.

void submit() {
    if (s_cmd.empty()) return;
    if (s_inFrame) {
        timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sab_process(s_cmd.data(), uint32_t(s_cmd.size()));
        if (fgEnabled()) fgTakeStream();
        clock_gettime(CLOCK_MONOTONIC, &t1);
        const double elapsed=double(t1.tv_sec - t0.tv_sec) + double(t1.tv_nsec - t0.tv_nsec) * 1e-9;
        s_submitSeconds += elapsed;
        s_submitTotalSeconds += elapsed;
    }
    s_cmd.clear();
}

// ------------------------------------------------------------------ pixel metrics
// On the console the pollution count is asynchronous: the game draws a layer,
// sets a draw-sync token, and the token interrupt calls back into the game,
// which reads the metric. Here every count of a frame goes to its own counter
// slot (Aurora's shaders count the fragments that pass the alpha test); the
// callback gets the newest finished count of the same token, normally one or
// two frames old. A token never counted yet reads as 0xFFFFFFFF plus the
// triangles' edge pixels, which the game's own subtraction turns back into its
// "unknown" value, -1.
struct MetricSlot {
    int token = -1;
    uint32_t tris = 0;
    float drawScale = 1.f;  // widescreen: the counted draws' horizontal scale (GX pixels per EFB pixel column)
    float pixelArea = 0.f;  // native data passes count 1:1; zero uses the frame's display scale
};
std::vector<MetricSlot> s_metricSlots;                       // this frame's
struct MetricFrame {
    std::vector<MetricSlot> slots;
    float area;  // EFB pixels per GX pixel, taken while the frame was recorded
};
std::map<uint64_t, MetricFrame> s_metricFrames;  // frames waiting for their counts
std::unordered_map<uint16_t, uint32_t> s_tokenValues;
bool s_metricActive = false;
int s_metricReadToken = -1;
uint64_t s_frameId = 0;
bool s_frameIdKnown = true;
uint64_t curFrameId() {
    if (!s_frameIdKnown) {
        s_frameId = s_inFrame ? sab_frame_id() : 0;
        s_frameIdKnown = true;
    }
    return s_frameId;
}

void metricStop() {
    if (!s_metricActive) return;
    s_metricActive = false;
    putAurora(0x0100);  // GX_LOAD_AURORA_PIXEL_METRIC
    put8(0);
}

void endFrameMetrics() {
    metricStop();
    if (!s_metricSlots.empty() && curFrameId())
        s_metricFrames[curFrameId()] = MetricFrame{std::move(s_metricSlots), sab_efb_pixel_area_scale()};
    s_metricSlots.clear();
}

void collectPixelMetrics() {
    uint64_t frameId;
    const uint32_t* counts;
    while (sab_poll_pixel_metrics(&frameId, &counts)) {
        for (auto it = s_metricFrames.begin(); it != s_metricFrames.end() && it->first <= frameId;) {
            if (it->first == frameId) {
                const auto& slots = it->second.slots;
                const float area = it->second.area;
                for (size_t i = 0; i < slots.size() && i < 255; i++) {
                    if (slots[i].token < 0) continue;
                    const float slotArea = slots[i].pixelArea > 0.f ? slots[i].pixelArea : area;
                    const double perPixel = double(slotArea > 0 ? slotArea : 1.0f) * double(slots[i].drawScale);
                    uint64_t v = uint64_t(double(counts[i]) / perPixel + 0.5) +
                                 uint64_t(slots[i].tris) * 4;
                    s_tokenValues[uint16_t(slots[i].token)] = v > 0xFFFFFFFFu ? 0xFFFFFFFFu : uint32_t(v);
                }
            }
            it = s_metricFrames.erase(it);
        }
    }
}

// ------------------------------------------------------------------ EFB copy write-back
// Texture copies stay on the GPU for drawing, but the game also reads some of
// them from RAM: Delfino's pollution layers are drawn into the EFB, copied
// back into the layer textures, and TPollutionLayer::isPolluted reads those
// bytes. So each texture copy is read back (a frame or two later), converted
// as GXCopyTex would (format, intensity) and stored in the game's memory,
// unless the CPU changed those bytes in the meantime.
struct PendingCopy {
    uint64_t requestId;
    const void* dest;
    uint64_t frameId;
    uint64_t hash;  // the destination's bytes when the copy was made (or last written back)
    uint32_t width, height, bytes, fmt, layout;
    bool intensity, alphaOne;
};
std::map<uint64_t, PendingCopy> s_pendingCopies;
std::unordered_map<const void*, uint64_t> s_appliedCopyRequests;
uint64_t s_nextCopyRequest = 0;
std::unordered_set<const void*> s_efbCopyDestinations;
uint32_t s_writeBackCount = 0;  // statistics

// Each result, including a failed request, retires only its own metadata.
// Frame/address alone cannot identify multiple copies to one texture in a frame.
bool takeCopyReadback(uint64_t requestId, const void* dest, uint32_t width,
                      uint32_t height, bool success, const uint32_t* rgba, PendingCopy& pc) {
    auto it = s_pendingCopies.find(requestId);
    if (it == s_pendingCopies.end()) return false;
    pc = it->second;
    s_pendingCopies.erase(it);
    if (!success || !rgba || dest != pc.dest || width != pc.width || height != pc.height) return false;
    const auto applied = s_appliedCopyRequests.find(dest);
    if (applied != s_appliedCopyRequests.end() && requestId <= applied->second) return false;
    // A delayed GPU copy must not overwrite a newer CPU edit of its texture.
    return hashBytes(dest, pc.bytes) == pc.hash;
}

void finishCopyReadback(const PendingCopy& pc, uint64_t storedHash) {
    s_appliedCopyRequests[pc.dest] = pc.requestId;
    for (auto it = s_pendingCopies.upper_bound(pc.requestId); it != s_pendingCopies.end(); ++it) {
        auto& later = it->second;
        // Our own write is not a CPU edit for later copies of the same bytes.
        if (later.dest == pc.dest && later.bytes == pc.bytes && later.hash == pc.hash)
            later.hash = storedHash;
    }
}

bool copyWriteBackEnabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("SMS_GX_COPY_WRITEBACK");
        on = !(e && e[0] == '0');
    }
    return on != 0;
}

bool copyWriteBackAll() {
    static const bool all = [] {
        const char* e = getenv("SMS_GX_COPY_WRITEBACK");
        return e && strcmp(e, "all") == 0;
    }();
    return all;
}

inline uint8_t quant(uint8_t v, int bits) {
    const int m = (1 << bits) - 1;
    return uint8_t(((v * m + 127) / 255) * 255 / m);
}

// The GX copy conversion of one RGBA8 pixel (as the OpenGL copy shader in gx_render.cpp does it).
void convertCopyPixel(uint8_t* c, uint32_t f, bool intensity, bool alphaOne) {
    if (alphaOne) c[3] = 255;
    const uint8_t r = c[0], g8 = c[1], b = c[2], a = c[3];
    uint8_t o[4] = {r, g8, b, a};
    if (intensity) {
        float y = 0.257f * r + 0.504f * g8 + 0.098f * b + 16.0f;
        uint8_t Y = uint8_t(y > 255.0f ? 255.0f : y + 0.5f);
        if (f == 0) o[0] = o[1] = o[2] = o[3] = quant(Y, 4);
        else if (f == 1) o[0] = o[1] = o[2] = o[3] = Y;
        else if (f == 2) { o[0] = o[1] = o[2] = quant(Y, 4); o[3] = quant(a, 4); }
        else if (f == 3) { o[0] = o[1] = o[2] = Y; o[3] = a; }
    } else {
        switch (f) {
        case 0: o[0] = o[1] = o[2] = o[3] = quant(r, 4); break;
        case 1: o[0] = o[1] = o[2] = o[3] = r; break;
        case 2: o[0] = o[1] = o[2] = quant(r, 4); o[3] = quant(a, 4); break;
        case 3: o[0] = o[1] = o[2] = r; o[3] = a; break;
        case 4: o[0] = quant(r, 5); o[1] = quant(g8, 6); o[2] = quant(b, 5); o[3] = 255; break;
        case 5:
            if (a > 252) { o[0] = quant(r, 5); o[1] = quant(g8, 5); o[2] = quant(b, 5); o[3] = 255; }
            else { o[0] = quant(r, 4); o[1] = quant(g8, 4); o[2] = quant(b, 4); o[3] = quant(a, 3); }
            break;
        case 7: o[0] = o[1] = o[2] = o[3] = a; break;
        case 8: o[0] = o[1] = o[2] = o[3] = r; break;
        case 9: o[0] = o[1] = o[2] = o[3] = g8; break;
        case 10: o[0] = o[1] = o[2] = o[3] = b; break;
        case 11: o[0] = o[1] = o[2] = g8; o[3] = r; break;
        case 12: o[0] = o[1] = o[2] = b; o[3] = g8; break;
        default: break;
        }
    }
    memcpy(c, o, 4);
}

// Frame dumps, as the OpenGL port writes them (SMS_GX_DUMP_DIR, SMS_GX_DUMP_EVERY):
// the displayed EFB is copied and read back like a texture copy, keyed by a marker.
uint8_t s_dumpMarkers[32];
struct PendingDump {
    uint32_t frame;
    std::string dir;
    uint64_t requestId;
};
std::unordered_map<const void*, PendingDump> s_dumpPending;  // marker -> frame and folder
// F9: the current frame and the next two seconds go to artifacts/aurora-captures/<time>/.
std::string s_captureDir;
uint32_t s_captureUntil = 0;
uint32_t s_dumpNext = 0;

const char* dumpDir() {
    static const char* dir = getenv("SMS_GX_DUMP_DIR");
    return dir && *dir ? dir : nullptr;
}
int dumpEvery() {
    static const int every = getenv("SMS_GX_DUMP_EVERY") ? atoi(getenv("SMS_GX_DUMP_EVERY")) : 1;
    return every > 0 ? every : 1;
}

bool writeFrameDump(const void* marker, uint32_t w, uint32_t h, const uint32_t* rgba,
                    uint64_t requestId, bool success) {
    auto it = s_dumpPending.find(marker);
    if (it == s_dumpPending.end()) return false;
    if (it->second.requestId != requestId) return true;  // a stale marker result cannot retire its replacement
    char path[1024];
    snprintf(path, sizeof path, "%s/frame_%06u.ppm", it->second.dir.c_str(), it->second.frame);
    if (FILE* f = success && rgba && w && h ? fopen(path, "wb") : nullptr) {
        fprintf(f, "P6\n%u %u\n255\n", w, h);
        std::vector<uint8_t> row(size_t(w) * 3);
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                const uint32_t c = rgba[size_t(y) * w + x];
                row[x * 3] = uint8_t(c);
                row[x * 3 + 1] = uint8_t(c >> 8);
                row[x * 3 + 2] = uint8_t(c >> 16);
            }
            fwrite(row.data(), 1, row.size(), f);
        }
        fclose(f);
    }
    s_dumpPending.erase(it);
    sab_destroy_copy_tex(const_cast<void*>(marker));
    return true;
}

void collectDepthCopy();

void storeCopyReadbacks() {
    const void* dest;
    uint32_t w, h;
    uint64_t frameId, requestId;
    int success;
    const uint32_t* rgba;
    static std::vector<uint8_t> px, enc;
    while (sab_poll_copy_readback_request(&dest, &w, &h, &frameId, &rgba, &requestId, &success)) {
        if (writeFrameDump(dest, w, h, rgba, requestId, success != 0)) continue;
        PendingCopy pc{};
        if (!takeCopyReadback(requestId, dest, w, h, success != 0, rgba, pc)) continue;
        if (texLevelBytes(pc.layout, w, h) != pc.bytes) continue;
        px.resize(size_t(w) * h * 4);
        memcpy(px.data(), rgba, px.size());
        for (size_t i = 0; i < px.size(); i += 4) convertCopyPixel(&px[i], pc.fmt, pc.intensity, pc.alphaOne);
        enc.resize(texLevelBytes(pc.layout, w, h));
        uint32_t n = encodeTexture(px.data(), pc.layout, w, h, enc.data());
        if (n != pc.bytes) continue;
        memcpy(const_cast<void*>(dest), enc.data(), n);
        const uint64_t stored = hashBytes(dest, pc.bytes);
        finishCopyReadback(pc, stored);
        GXPC_InvalidateRange(dest, pc.bytes);
        s_writeBackCount++;
    }
    collectDepthCopy();
}

// The bathtub's Z24X8 depth copy (Corona Mountain's boss): the CPU builds the
// water surface from it. Waiting for the GPU at every copy cost about 9 ms of
// each frame, so after a first synchronous copy it is asked for without
// waiting and stored into RAM once the GPU has it, usually by the next frame.
// The console's GP also ran behind the CPU; the surface just lags the drops
// by a frame. SMS_AURORA_SYNC_DEPTH=1 restores the waiting copy.
struct DepthCopy {
    uint64_t tag = 0;
    void* dest = nullptr;
    uint32_t bytes = 0;
    uint64_t hash = 0;
    uint16_t width = 0, height = 0, dstWidth = 0, dstHeight = 0;
};
DepthCopy s_depthPending;
const void* s_depthStoredDest = nullptr;
uint64_t s_depthStoredFrame = 0, s_depthNextTag = 0;

bool asyncDepthCopies() {
    static const bool on = [] {
        const char* v = getenv("SMS_AURORA_SYNC_DEPTH");
        return !v || !*v || strcmp(v, "0") == 0;
    }();
    return on;
}

uint32_t z24x8Bytes(uint32_t w, uint32_t h) { return ((w + 3) / 4) * ((h + 3) / 4) * 64; }

// Stores a finished copy if its RAM still holds what it held when the copy was
// asked for: a stage change frees the heap, and the buffer may be reused.
void collectDepthCopy() {
    if (!s_depthPending.tag) return;
    static std::vector<uint8_t> scratch;
    scratch.resize(s_depthPending.bytes);
    const int r = sab_take_depth_z24x8(s_depthPending.tag, scratch.data(), s_depthPending.width,
                                       s_depthPending.height, s_depthPending.dstWidth, s_depthPending.dstHeight);
    if (r == 0) return;
    if (r == 1 && hashBytes(s_depthPending.dest, s_depthPending.bytes) == s_depthPending.hash) {
        memcpy(s_depthPending.dest, scratch.data(), s_depthPending.bytes);
        s_depthStoredDest = s_depthPending.dest;
        s_depthStoredFrame = curFrameId();
    }
    s_depthPending = {};
}

bool copyDepthZ24X8(void* dest, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t dw, uint16_t dh) {
    collectDepthCopy();
    // While a copy is in flight the exact-depth slot is busy: keep that one.
    if (s_depthPending.tag) return true;
    const bool warm = s_depthStoredDest == dest && curFrameId() - s_depthStoredFrame <= 4;
    if (!asyncDepthCopies() || !warm) {
        if (!sab_copy_depth_z24x8_sync(dest, x, y, w, h, dw, dh)) return false;
        s_depthStoredDest = dest;
        s_depthStoredFrame = curFrameId();
        return true;
    }
    const uint64_t tag = ++s_depthNextTag;
    if (!sab_copy_depth_z24x8_async(x, y, w, h, tag)) return false;
    s_depthPending = {tag, dest, z24x8Bytes(dw, dh), 0, w, h, dw, dh};
    s_depthPending.hash = hashBytes(dest, s_depthPending.bytes);
    return true;
}

void ensureCopyReadbackCapacity() {
    const auto capacity = sab_copy_readback_capacity();
    if (capacity <= 1 || s_pendingCopies.size() + s_dumpPending.size() < capacity - 1) return;
    // Completed results still count as frontend pending until polled. Retire
    // those first; keep counting queued/unencoded requests before admitting more.
    storeCopyReadbacks();
    if (s_pendingCopies.size() + s_dumpPending.size() >= capacity - 1 && sab_flush_copy_readbacks())
        storeCopyReadbacks();
}

// ------------------------------------------------------------------ statistics
struct Stats {
    uint32_t draws = 0, vertices = 0, texLoads = 0, texRehash = 0, copies = 0;
    double submitSeconds = 0, presentSeconds = 0;
} s_stats;
int s_statsEvery = 0;
std::vector<double> s_frameTimes;
double s_lastFrameEnd = 0, s_presentAccum = 0;
// Time spent getting the next swapchain image (the display's vsync wait), and how
// many frames waited long enough to make the game miss a retrace.
double s_acquireAccum = 0, s_acquireLast = 0;
int s_acquireLong = 0;
// SMS_GX_FRAMELOG=<file>: one line per frame (timing breakdown and retraces
// it spanned), to find what makes a frame miss a retrace.
FILE* s_frameLog = nullptr;
// Optional diagnostic counters; normal play avoids the extra fine-grained clocks.
FILE* s_profileLog = nullptr;
bool s_profileFine = false;
struct FrameProfile {
    double draw = 0, textures = 0, peek = 0;
    uint32_t draws = 0, peeks = 0;
} s_profile;
struct ProfileScope {
    double& total;
    double start;
    explicit ProfileScope(double& value) : total(value), start(s_profileFine ? nowSeconds() : 0) {}
    ~ProfileScope() { if (s_profileFine) total += nowSeconds() - start; }
};

// ------------------------------------------------------------------ vertex format 0
uint32_t s_curFmt = ~0u;  // the VF_* format the CP registers of Aurora hold

void setVertexFormat(uint32_t fmt) {
    if (fmt == s_curFmt) return;
    s_curFmt = fmt;
    const bool mtx = fmt & VF_MTX;
    uint32_t vcdLo = 0, vcdHi = 0;
    if (mtx) vcdLo |= 0x1FF;               // PNMTXIDX + TEX0-7MTXIDX, direct
    vcdLo |= 1u << 9;                      // POS direct
    if (fmt & VF_NRM) vcdLo |= 1u << 11;   // NRM direct
    if (fmt & VF_CLR0) vcdLo |= 1u << 13;  // CLR0 direct
    if (fmt & (VF_CLR0 << 1)) vcdLo |= 1u << 15;
    for (int t = 0; t < 8; t++)
        if (fmt & (VF_TEX0 << t)) vcdHi |= 1u << (t * 2);
    // POS xyz f32, NRM (or NBT) f32, CLR rgba8, TEX st f32
    uint32_t vatA = 1u | 4u << 1;
    vatA |= ((fmt & VF_NBT) ? 1u : 0u) << 9 | 4u << 10;
    vatA |= 1u << 13 | 5u << 14;
    vatA |= 1u << 17 | 5u << 18;
    vatA |= 1u << 21 | 4u << 22;
    uint32_t vatB = 1u | 4u << 1;          // TEX1
    vatB |= 1u << 9 | 4u << 10;            // TEX2
    vatB |= 1u << 18 | 4u << 19;           // TEX3
    vatB |= 1u << 27 | 4u << 28;           // TEX4
    vatB |= 1u << 31;                      // vertex cache enhance
    uint32_t vatC = 1u << 5 | 4u << 6;     // TEX5
    vatC |= 1u << 14 | 4u << 15;           // TEX6
    vatC |= 1u << 23 | 4u << 24;           // TEX7
    putCP(0x50, vcdLo);
    putCP(0x60, vcdHi);
    putCP(0x70, vatA);
    putCP(0x80, vatB);
    putCP(0x90, vatC);
}

// ------------------------------------------------------------------ textures
struct TexKey {
    uintptr_t ptr;
    uint32_t image0;  // width, height, format
    uint32_t levels;
    bool operator==(const TexKey& o) const { return ptr == o.ptr && image0 == o.image0 && levels == o.levels; }
};
struct TexKeyHash {
    size_t operator()(const TexKey& k) const {
        return std::hash<uint64_t>()(uint64_t(k.ptr) * 0x9E3779B97F4A7C15ull ^ uint64_t(k.image0) << 8 ^ k.levels);
    }
};
struct TexEntry {
    uint32_t id, version;
    uint32_t copyVersion = 0;
    uint64_t hash;
    uint32_t bytes;
    uint32_t checkedGen;
    bool dirty;
};
std::unordered_map<TexKey, TexEntry, TexKeyHash> s_textures;
std::multimap<uintptr_t, TexEntry*> s_texByAddr;
std::unordered_map<const void*, uint32_t> s_copyVersions;
uint32_t s_nextTexId = 0x40000000u;
uint32_t s_texGen = 1;

struct TlutEntry {
    uint32_t id;
    std::vector<uint8_t> data;
};
std::unordered_map<uint64_t, std::unique_ptr<TlutEntry>> s_tluts;
uint32_t s_nextTlutId = 0x40000000u;

struct MapState {
    uint32_t id = 0, version = 0, tlutId = 0;
    bool regsDirty = true;
    // unordered_map node references survive rehash; entries live until shutdown.
    TexEntry* entry = nullptr;
    const uint8_t* data = nullptr;
    uint32_t image0 = 0, levels = 0;
    const TlutEntry* palette = nullptr;
    uint32_t paletteReg = 0, paletteEntries = 0;
};
MapState s_maps[8];
bool prepCacheEnabled() {
    static const bool enabled=[] { const char* v=getenv("SMS_GX_PREP_CACHE"); return !v || strcmp(v,"0"); }();
    return enabled;
}
bool stateCacheEnabled() {
    // Frame generation plays a frame's stream again on its own: a write skipped
    // because an earlier frame had sent the same value would be missing there.
    static const bool enabled=[] { const char* v=getenv("SMS_GX_STATE_CACHE"); return (!v || strcmp(v,"0")) && !fgEnabled(); }();
    return enabled;
}

uint32_t texLevels(int map) {
    uint32_t mode0 = g.bp[bpTexReg(BP_TX_MODE0, map)];
    uint32_t mode1 = g.bp[bpTexReg(BP_TX_MODE1, map)];
    uint32_t minFilt = (mode0 >> 5) & 7;
    if ((minFilt & 3) == 0) return 1;  // no mipmapping
    return ((mode1 >> 8) & 0xFF) / 16 + 1;
}

TexEntry& textureEntry(const uint8_t* ptr, uint32_t image0, uint32_t levels) {
    TexKey key{reinterpret_cast<uintptr_t>(ptr), image0 & 0xFFFFFF, levels};
    auto it = s_textures.find(key);
    if (it != s_textures.end()) return it->second;
    uint32_t w = (image0 & 0x3FF) + 1, h = ((image0 >> 10) & 0x3FF) + 1, fmt = (image0 >> 20) & 0xF;
    TexEntry e{};
    e.id = s_nextTexId++;
    e.version = 1;
    e.bytes = GXGetTexBufferSize(u16(w), u16(h), fmt, levels > 1, u8(levels));
    e.hash = e.bytes ? hashBytes(ptr, e.bytes) : 0;
    e.checkedGen = s_texGen;
    auto ins = s_textures.emplace(key, e);
    s_texByAddr.emplace(key.ptr, &ins.first->second);
    return ins.first->second;
}

void recheck(TexEntry& e, const uint8_t* ptr) {
    auto cvIt = s_copyVersions.find(ptr);
    if (cvIt != s_copyVersions.end() && e.copyVersion != cvIt->second) {
        e.copyVersion = cvIt->second;
        e.version++;
        e.dirty = false;
        e.checkedGen = s_texGen;
        return;
    }
    if (!e.dirty && e.checkedGen == s_texGen) return;
    e.dirty = false;
    e.checkedGen = s_texGen;
    if (!e.bytes) return;
    uint64_t h = hashBytes(ptr, e.bytes);
    s_stats.texRehash++;
    if (h != e.hash) {
        e.hash = h;
        e.version++;
    }
}

uint32_t tlutFor(int map, uint32_t fmt, const uint8_t** data, uint16_t* entries, uint32_t* tlutFmt) {
    uint32_t reg = g.bp[bpTexReg(BP_TX_TLUT, map)];
    uint32_t off = (reg & 0x3FF) << 9;
    *tlutFmt = (reg >> 10) & 3;
    uint32_t n = fmt == GX_TF_C4 ? 16 : fmt == GX_TF_C8 ? 256 : 16384;
    if (off + n * 2 > TMEM_TLUT_SIZE) n = (TMEM_TLUT_SIZE - off) / 2;
    const uint8_t* src = g.tlutMem + off;
    auto& ms = s_maps[map];
    // Both FIFO loads and GXLoadTlut write TMEM. Compare its current bytes,
    // rather than assuming that the texture address or frame identifies a palette.
    // This also catches overlapping loads into the middle of a palette.
    if (stateCacheEnabled() && ms.palette && ms.paletteReg == reg && ms.paletteEntries == n &&
        std::memcmp(src, ms.palette->data.data(), n * 2) == 0) {
        *data = ms.palette->data.data();
        *entries = uint16_t(n);
        return ms.palette->id;
    }
    uint64_t h = hashBytes(src, n * 2, uint64_t(*tlutFmt) << 32 | n);
    auto& slot = s_tluts[h];
    if (!slot) {
        slot.reset(new TlutEntry);
        slot->id = s_nextTlutId++;
        slot->data.assign(src, src + n * 2);
    }
    ms.palette = slot.get();
    ms.paletteReg = reg;
    ms.paletteEntries = n;
    *data = slot->data.data();
    *entries = uint16_t(n);
    return slot->id;
}

// The texture maps the current TEV and indirect stages sample.
uint32_t usedMaps() {
    uint32_t gen = g.bp[BP_GENMODE];
    int stages = int((gen >> 10) & 0xF) + 1;
    int ind = int((gen >> 16) & 7);
    uint32_t mask = 0;
    for (int s = 0; s < stages; s++) {
        uint32_t r = g.bp[BP_TREF + s / 2];
        uint32_t v = (s & 1) ? r >> 12 : r;
        if (v & 0x40) mask |= 1u << (v & 7);
    }
    uint32_t iref = g.bp[BP_RAS1_IREF];
    for (int i = 0; i < ind; i++) mask |= 1u << ((iref >> (i * 6)) & 7);
    return mask;
}

void syncTextures() {
    ProfileScope timing(s_profile.textures);
    uint32_t mask = usedMaps();
    for (int m = 0; m < 8; m++) {
        if (!(mask & (1u << m))) continue;
        const uint8_t* ptr = g.texImage[m];
        if (!ptr) continue;
        uint32_t image0 = g.bp[bpTexReg(BP_TX_IMAGE0, m)];
        uint32_t levels = texLevels(m);
        MapState& ms = s_maps[m];
        if(!prepCacheEnabled() || !ms.entry || ms.data!=ptr || ms.image0!=image0 || ms.levels!=levels) {
            ms.entry=&textureEntry(ptr,image0,levels);
            ms.data=ptr; ms.image0=image0; ms.levels=levels;
        }
        TexEntry& e = *ms.entry;
        recheck(e, ptr);
        uint32_t fmt = (image0 >> 20) & 0xF;
        uint32_t tlutId = 0;
        const uint8_t* tlutData = nullptr;
        uint16_t tlutEntries = 0;
        uint32_t tlutFmt = 0;
        bool ci = fmt == GX_TF_C4 || fmt == GX_TF_C8 || fmt == GX_TF_C14X2;
        if (ci) tlutId = tlutFor(m, fmt, &tlutData, &tlutEntries, &tlutFmt);
        if (!ms.regsDirty && ms.id == e.id && ms.version == e.version && ms.tlutId == tlutId) continue;
        static const bool texLog = getenv("SMS_AURORA_TEXLOG") != nullptr;
        if (texLog && e.version == 1 && ms.id != e.id)
            fprintf(stderr, "[gx-aurora] tex id %08x map %d fmt %u %ux%u levels %u tlut %u/%u entries %u data %p\n",
                    e.id, m, fmt, (image0 & 0x3FF) + 1, ((image0 >> 10) & 0x3FF) + 1, levels, ci ? tlutId : 0,
                    tlutFmt, tlutEntries, (const void*)ptr);
        if (ci && ms.tlutId != tlutId) {
            putAurora(0x0031);  // GX_LOAD_AURORA_TLUT
            put8(uint8_t(m));
            put64(reinterpret_cast<uint64_t>(tlutData));
            put32(tlutFmt);
            put16(tlutEntries);
            put32(tlutId);
            put32(1);
        }
        putAurora(0x0030);  // GX_LOAD_AURORA_TEXOBJ
        put8(uint8_t(m));
        put64(reinterpret_cast<uint64_t>(ptr));
        put32((image0 & 0x3FF) + 1);
        put32(((image0 >> 10) & 0x3FF) + 1);
        put32(fmt);
        put32(ci ? uint32_t(m) : 0);  // GX_TLUT0 + map: one TLUT slot per map
        put8(levels > 1 ? 1 : 0);
        put32(e.id);
        put32(e.version);
        ms.id = e.id;
        ms.version = e.version;
        ms.tlutId = tlutId;
        ms.regsDirty = false;
        s_stats.texLoads++;
    }
}

// Texture-coordinate scale (SU_SSIZE/SU_TSIZE, BP 0x30-0x3F). The SDK writes them
// before every draw for each coordinate a TEV or indirect stage uses, from the
// size and wrap of the texture bound with it (__GXSetSUTexRegs), unless the
// coordinate is scaled manually. The port's front end only writes manual ones,
// as its OpenGL renderer reads the texture sizes itself; Aurora computes its
// indirect texturing in texel space from these registers, so they are rebuilt
// here (FLUDD's water, the goop's swirl).
uint32_t s_suSent[16];
bool s_suValid[16];

void suFor(uint32_t map, uint32_t coord) {
    if (map > 7 || coord > 7 || g.tcManual[coord]) return;
    const uint32_t image0 = g.bp[bpTexReg(BP_TX_IMAGE0, int(map))];
    const uint32_t mode0 = g.bp[bpTexReg(BP_TX_MODE0, int(map))];
    const uint32_t w = image0 & 0x3FF, h = (image0 >> 10) & 0x3FF;
    const uint32_t sBias = (mode0 & 3) == 1, tBias = ((mode0 >> 2) & 3) == 1;
    const uint32_t regS = BP_SU_SSIZE + 2 * coord, regT = regS + 1;
    const uint32_t valS = (g.bp[regS] & ~0x1FFFFu) | w | sBias << 16;
    const uint32_t valT = (g.bp[regT] & ~0x1FFFFu) | h | tBias << 16;
    for (const auto& [reg, val] : {std::pair{regS, valS}, std::pair{regT, valT}}) {
        const uint32_t slot = reg - BP_SU_SSIZE;
        if (s_suValid[slot] && s_suSent[slot] == val) continue;
        putBP(reg, val);
        s_suSent[slot] = val;
        s_suValid[slot] = true;
    }
}

void syncTexCoordScales() {
    const uint32_t gen = g.bp[BP_GENMODE];
    const uint32_t stages = ((gen >> 10) & 0xF) + 1, ind = (gen >> 16) & 7;
    const uint32_t iref = g.bp[BP_RAS1_IREF];
    for (uint32_t i = 0; i < ind; i++) suFor((iref >> (i * 6)) & 7, (iref >> (i * 6 + 3)) & 7);
    for (uint32_t i = 0; i < stages; i++) {
        const uint32_t order = (g.bp[BP_TREF + i / 2] >> ((i & 1) * 12)) & 0xFFF;
        if (order & 0x40) suFor(order & 7, (order >> 3) & 7);  // texture enabled
    }
}

// ------------------------------------------------------------------ primitives
std::vector<uint8_t> s_prim;
uint32_t s_primFmt = 0, s_primStride = 0;

// Aurora decodes inline vertex data as the GameCube stores it, big-endian,
// whatever the byte order of the surrounding command stream.
inline void copyBE32_3(uint8_t* out, const uint8_t* in) {
    uint32_t v0, v1, v2;
    std::memcpy(&v0, in, 4);
    std::memcpy(&v1, in + 4, 4);
    std::memcpy(&v2, in + 8, 4);
    v0 = __builtin_bswap32(v0);
    v1 = __builtin_bswap32(v1);
    v2 = __builtin_bswap32(v2);
    std::memcpy(out, &v0, 4);
    std::memcpy(out + 4, &v1, 4);
    std::memcpy(out + 8, &v2, 4);
}

inline void copyBE32_2(uint8_t* out, const uint8_t* in) {
    uint32_t v0, v1;
    std::memcpy(&v0, in, 4);
    std::memcpy(&v1, in + 4, 4);
    v0 = __builtin_bswap32(v0);
    v1 = __builtin_bswap32(v1);
    std::memcpy(out, &v0, 4);
    std::memcpy(out + 4, &v1, 4);
}

inline void copyBE32(uint8_t* out, const uint8_t* in, uint32_t words) {
    if (words == 3) { copyBE32_3(out, in); return; }
    if (words == 2) { copyBE32_2(out, in); return; }
    for (uint32_t i = 0; i < words; i++) {
        uint32_t v;
        std::memcpy(&v, in + i * 4, 4);
        v = __builtin_bswap32(v);
        std::memcpy(out + i * 4, &v, 4);
    }
}


struct DrawMapping { float scale=1.f, offset=0.f; };
float xfFloat(uint32_t reg) { float value;std::memcpy(&value,&g.xfReg[reg],4);return value; }
void emitXfFloat(uint32_t reg,float value) {
    uint32_t bits;std::memcpy(&bits,&value,4);
    put8(0x10);put32(0x1000+reg);put32(bits);
}
// Aurora rebuilds its viewport only from a write of all six XF viewport words.
void emitViewport(float scaleX, float offsetX) {
    float vp[6];
    std::memcpy(vp, &g.xfReg[XFR_VIEWPORT], sizeof vp);
    vp[0] *= scaleX;
    vp[3] = (vp[3] - 342.f) * scaleX + offsetX + 342.f;
    put8(0x10);
    put32((6 - 1) << 16 | (0x1000 + XFR_VIEWPORT));
    for (float f : vp) {
        uint32_t bits;
        std::memcpy(&bits, &f, 4);
        put32(bits);
    }
}
// Same per-draw mapping as the OpenGL port, expressed in Aurora's logical
// EFB coordinates. Its physical render target already has the wide aspect.
DrawMapping drawMapping(uint32_t count) {
    DrawMapping result;
    if(s_widescreen<=1.0001f)return result;
    const float wide=s_widescreen, margin=320.f*(wide-1.f);
    const bool ortho=(g.xfReg[XFR_PROJ+6]&1)!=0;
    if(s_stretch2D&&ortho)return result;
    // Orthographic depth-only passes produce data for a logical EFB copy
    // (the bathtub height field). Moving their vertices/scissor into the HUD
    // frame leaves the CPU's unshifted depth-copy rectangle empty.
    const uint32_t writes = g.bp[BP_CMODE0];
    const bool depthOnly = !(writes & ((1u << 3) | (1u << 4))) && (g.bp[BP_ZMODE] & (1u << 4));
    if (ortho && depthOnly) return result;
    const float sx=xfFloat(XFR_VIEWPORT),cx=xfFloat(XFR_VIEWPORT+3)-342.f;
    const bool full=std::fabs(std::fabs(sx)*2.f-640.f)<2.f&&std::fabs(cx-320.f)<2.f;
    // Sub-viewports keep their existing mapping. Avoid transforming every
    // vertex of pollution/data draws only to discard the computed bounds.
    if (!full) return result;
    if(full&&!ortho&&!s_hud) {
        const float p0=xfFloat(XFR_PROJ),p11=xfFloat(XFR_PROJ+2);
        const float aspect=p0!=0.f?std::fabs(p11/p0):0.f;
        constexpr float gameAspect=660.f*.91346145f/448.f;
        const float scale=aspect<gameAspect*std::sqrt(wide)?1.f/wide:1.f;
        return {scale,320.f*(1.f-scale)};
    }
    const auto& layout=vtxFmtLayout(s_primFmt);
    float lo=1e30f,hi=-1e30f;
    for(uint32_t v=0;v<count;++v) {
        const auto* vertex=s_prim.data()+size_t(v)*s_primStride;
        float position[3];std::memcpy(position,vertex,12);
        const uint32_t idx=(s_primFmt&VF_MTX)?(vertex[layout.mtx]&63):(g.xfReg[XFR_MATIDX_A]&63);
        if(idx*4+12>XF_MEM_WORDS)continue;
        float matrix[12];std::memcpy(matrix,&g.xfMem[idx*4],48);
        const float x=matrix[0]*position[0]+matrix[1]*position[1]+matrix[2]*position[2]+matrix[3];
        float screen;
        if(ortho)screen=cx+sx*(xfFloat(XFR_PROJ)*x+xfFloat(XFR_PROJ+1));
        else {
            const float z=matrix[8]*position[0]+matrix[9]*position[1]+matrix[10]*position[2]+matrix[11];
            if(z>=-1e-6f)continue;
            screen=cx+sx*(xfFloat(XFR_PROJ)*x+xfFloat(XFR_PROJ+1)*z)/-z;
        }
        lo=std::min(lo,screen);hi=std::max(hi,screen);
    }
    auto anchorOffset=[&](float center) {
        if(!s_hud)return margin;
        for(const auto& pane:s_hudPanes)if(pane.second-pane.first<480.f) {
            center=(pane.first+pane.second)*.5f;break;
        }
        return center<640.f/3.f?0.f:center>1280.f/3.f?2.f*margin:margin;
    };
    if(!ortho)return {1.f/wide,anchorOffset(lo<=hi?(lo+hi)*.5f:320.f)/wide};
    bool artwork=false;
    const bool color=(g.bp[BP_CMODE0]>>3)&1;
    const uint32_t stages=((g.bp[BP_GENMODE]>>10)&15)+1;
    for(uint32_t st=0;color&&st<stages&&!artwork;++st) {
        const uint32_t order=(g.bp[BP_TREF+(st>>1)]>>((st&1)*12))&0x3ff;
        if(!(order&0x40))continue;
        const auto map=order&7;
        const auto image=g.bp[bpTexReg(BP_TX_IMAGE0,map)];
        const auto* data=g.texImage[map];
        if(data&&(image&0x3ff)+1>=32&&((image>>10)&0x3ff)+1>=32&&
           s_efbCopyDestinations.find(data)==s_efbCopyDestinations.end())artwork=true;
    }
    const bool spans=lo<=2.f&&hi>=638.f;
    if(spans&&!artwork)return result;
    return {1.f/wide,(spans?margin:anchorOffset(lo<=hi?(lo+hi)*.5f:320.f))/wide};
}

// A draw's vertices into the stream. Aurora's layout is the loader's with the
// matrix indices first (9 bytes) instead of last, and every float (all but the
// colours) byte-swapped: each vertex is at most four runs, matrix indices,
// floats before the colours, the colours, floats after them.
void emitVertices(uint8_t* out, const uint8_t* in, uint32_t count, uint32_t fmt, const VtxFmtLayout& L) {
    const bool mtx = fmt & VF_MTX;
    const uint32_t data = mtx ? L.mtx : L.stride;  // the loader's bytes before the matrix indices
    uint32_t clr = data, clrBytes = 0;
    for (int c = 1; c >= 0; c--)
        if (fmt & (VF_CLR0 << c)) {
            clr = L.clr[c];
            clrBytes += 4;
        }
    const uint32_t after = clr + clrBytes;
    auto swapWords = [](uint8_t* o, const uint8_t* i, uint32_t bytes) {
        for (uint32_t k = 0; k < bytes; k += 4) {
            uint32_t w;
            std::memcpy(&w, i + k, 4);
            w = __builtin_bswap32(w);
            std::memcpy(o + k, &w, 4);
        }
    };
    for (uint32_t v = 0; v < count; v++, in += s_primStride) {
        if (mtx) {
            std::memcpy(out, in + L.mtx, 9);
            out += 9;
        }
        swapWords(out, in, clr);
        std::memcpy(out + clr, in + clr, clrBytes);
        swapWords(out + after, in + after, data - after);
        out += data;
    }
}

// The attribute-by-attribute copy emitVertices replaced, for SMS_GX_EMIT_CHECK.
void emitVerticesReference(uint8_t* out, const uint8_t* in, uint32_t count, uint32_t fmt, const VtxFmtLayout& L) {
    const bool mtx = fmt & VF_MTX;
    const bool nrm = fmt & VF_NRM;
    const uint32_t nrmBytes = (fmt & VF_NBT) ? 36 : 12;
    for (uint32_t v = 0; v < count; v++, in += s_primStride) {
        if (mtx) {
            std::memcpy(out, in + L.mtx, 9);
            out += 9;
        }
        copyBE32_3(out, in);
        out += 12;
        if (nrm) {
            if (nrmBytes == 12) copyBE32_3(out, in + L.nrm);
            else copyBE32(out, in + L.nrm, nrmBytes / 4);
            out += nrmBytes;
        }
        for (int c = 0; c < 2; c++)
            if (fmt & (VF_CLR0 << c)) {
                std::memcpy(out, in + L.clr[c], 4);
                out += 4;
            }
        for (int t = 0; t < 8; t++)
            if (fmt & (VF_TEX0 << t)) {
                copyBE32_2(out, in + L.tex[t]);
                out += 8;
            }
    }
}

void emitDraw(uint8_t opcode, uint32_t count) {
    ProfileScope timing(s_profile.draw);
    if (s_profileLog) ++s_profile.draws;
    const VtxFmtLayout& L = vtxFmtLayout(s_primFmt);
    const uint32_t fmt = s_primFmt;
    const bool mtx = fmt & VF_MTX;
    const bool nrm = fmt & VF_NRM;
    const uint32_t nrmBytes = (fmt & VF_NBT) ? 36 : 12;
    static uint32_t strideFormat=~0u, outStride=0;
    if(!prepCacheEnabled() || strideFormat!=fmt) {
        outStride = (mtx ? 9 : 0) + 12 + (nrm ? nrmBytes : 0);
        for (int c = 0; c < 2; c++)
            if (fmt & (VF_CLR0 << c)) outStride += 4;
        for (int t = 0; t < 8; t++)
            if (fmt & (VF_TEX0 << t)) outStride += 8;
        strideFormat=fmt;
    }
    setVertexFormat(fmt);
    syncTextures();
    syncTexCoordScales();
    const auto mapping=drawMapping(count);
    const bool mapped=mapping.scale!=1.f||mapping.offset!=0.f;
    const uint32_t tl=g.bp[BP_SCISSOR_TL],br=g.bp[BP_SCISSOR_BR];
    if (s_metricActive && !s_metricSlots.empty()) {
        s_metricSlots.back().drawScale = mapping.scale;
        s_metricSlots.back().pixelArea = s_dataPassActive ? 1.f : 0.f;
    }
    if(mapped) {
        emitViewport(mapping.scale, mapping.offset);
        const int left=int((tl>>12)&0x7ff)-342,right=int((br>>12)&0x7ff)-342;
        if(!(left<=0&&right>=639)) {
            const int l=std::clamp(int(std::floor(left*mapping.scale+mapping.offset))+342,0,2047);
            const int r=std::clamp(int(std::ceil((right+1)*mapping.scale+mapping.offset))-1+342,0,2047);
            putBP(BP_SCISSOR_TL,(tl&~0x7ff000u)|(uint32_t(l)<<12));
            putBP(BP_SCISSOR_BR,(br&~0x7ff000u)|(uint32_t(r)<<12));
        }
    }
    if (fgEnabled() && s_inFrame) fgNoteDraw(opcode, count, fmt, L);
    // A draw holds at most 0xFFFF vertices; the loader never makes bigger ones.
    put8(uint8_t(opcode | 0));  // vertex attribute format 0
    put16(uint16_t(count));
    size_t base = s_cmd.size();
    s_cmd.resize(base + size_t(count) * outStride);
    emitVertices(&s_cmd[base], s_prim.data(), count, fmt, L);
    // SMS_GX_EMIT_CHECK: compare with the attribute-by-attribute copy.
    static const bool emitCheck = getenv("SMS_GX_EMIT_CHECK") != nullptr;
    if (emitCheck) {
        static std::vector<uint8_t> reference;
        static uint32_t checked = 0, mismatches = 0;
        reference.resize(size_t(count) * outStride);
        emitVerticesReference(reference.data(), s_prim.data(), count, fmt, L);
        checked++;
        if (std::memcmp(reference.data(), &s_cmd[base], reference.size()) != 0 && mismatches++ < 8)
            fprintf(stderr, "[gx-aurora] emit check: format %05x differs (%u draws checked)\n", fmt, checked);
        if (checked % 100000 == 0)
            fprintf(stderr, "[gx-aurora] emit check: %u draws, %u differ\n", checked, mismatches);
    }
    if(mapped) {
        emitViewport(1.f, 0.f);
        putBP(BP_SCISSOR_TL,tl);putBP(BP_SCISSOR_BR,br);
    }
    s_stats.draws++;
    s_stats.vertices += count;
    if (s_metricActive && !s_metricSlots.empty()) {
        uint32_t tris = 0;
        switch (opcode & 0xF8) {
        case 0x80: tris = count / 4 * 2; break;                         // quads
        case 0x90: tris = count / 3; break;                             // triangles
        case 0x98: case 0xA0: tris = count > 2 ? count - 2 : 0; break;  // strip, fan
        default: break;
        }
        s_metricSlots.back().tris += tris;
    }
    if (s_cmd.size() > (4u << 20)) submit();
}

// ------------------------------------------------------------------ frame generation
// What a frame did, in order, to do it again: the command stream chunks (with
// where each draw starts and the position matrices it uses), the texture and
// display copies, the data passes. Anything else (a depth copy, an EFB size
// change) marks the frame as not replayable. Everything lives in two frames
// (this one and the previous one) whose buffers are reused: a busy frame has
// 30,000 draws, and copying or reallocating per draw cost more than the replay.
struct FgDraw {
    uint64_t key;     // the same draw in another frame (see fgKey)
    uint32_t offset;  // in its chunk, where the draw command starts
    uint32_t row0;    // its first FgRow
    uint8_t nrows;
};
struct FgRow {
    uint32_t at;  // in the chunk, the row's 12 words in the XF load that set them; ~0u: none
    uint32_t row; // position-matrix row (XF memory row = 4 words)
    float m[12];
};
struct FgOp {
    enum Kind : uint8_t { Stream, CopyTex, CopyDisplay, DataPassBegin, DataPassEnd } kind;
    uint32_t buffer = 0, draw0 = 0, draw1 = 0;  // Stream: its chunk and draws
    void* dest = nullptr;
    uint16_t x = 0, y = 0, w = 0, h = 0, dw = 0, dh = 0;
    uint32_t fmt = 0;
    bool clear = false;
};
std::vector<std::vector<uint8_t>> s_fgPool;  // spare chunk buffers
struct FgFrame {
    std::vector<FgOp> ops;
    std::vector<std::vector<uint8_t>> buffers;
    std::vector<FgDraw> draws;
    std::vector<FgRow> rows;
    bool valid = true;
    bool shown = false;  // ends with a display copy
    void* xfb = nullptr;
    void clear() {
        for (auto& b : buffers) {
            b.clear();
            s_fgPool.push_back(std::move(b));
        }
        buffers.clear();
        ops.clear();
        draws.clear();
        rows.clear();
        valid = true;
        shown = false;
        xfb = nullptr;
    }
};
FgFrame s_fgFrames[2];
FgFrame* s_fgCur = &s_fgFrames[0];
FgFrame* s_fgPrev = &s_fgFrames[1];
uint32_t s_fgPendingDraw = 0;     // the first draw of s_cmd in s_fgCur->draws
std::vector<size_t> s_fgMetrics;  // metric slot bytes in s_cmd
double s_fgLastLive = 0, s_fgPeriod = 1.0 / 59.94;
uint32_t s_fgReplays = 0, s_fgMatched = 0, s_fgDrawsSeen = 0;
double s_fgSeconds = 0, s_fgBuild = 0, s_fgProcess = 0, s_fgFinish = 0;
uint32_t s_fgRows = 0, s_fgStale = 0;
// Where in s_cmd the latest full load of each position-matrix row is (valid
// while s_fgRowSerial matches s_fgSerial, the chunk being built).
uint32_t s_fgRowAt[64], s_fgRowSerial[64];
uint32_t s_fgSerial = 1;
bool s_fgAsyncBegin = false;  // a replay is queued: begin the next frame behind it
uint8_t s_fgMarkers[8];
uint32_t s_fgMarkerNext = 0;

void fgInvalidate() { if (fgEnabled()) s_fgCur->valid = false; }

void fgNoteMetric(size_t slotByte) { s_fgMetrics.push_back(slotByte); }

// An XF load into position-matrix memory: the rows it sets whole can be
// rewritten in place by the replay; the ones it sets in part cannot.
void fgNoteXf(uint32_t addr, uint32_t count, size_t dataAt) {
    if (addr >= 64 * 4) return;
    const uint32_t end = std::min<uint32_t>(addr + count, 64 * 4);
    for (uint32_t row = addr / 4; row * 4 < end; row++) {
        if (row * 4 >= addr && row * 4 + 12 <= addr + count) {
            s_fgRowAt[row] = uint32_t(dataAt + (row * 4 - addr) * 4);
            s_fgRowSerial[row] = s_fgSerial;
        } else {
            s_fgRowSerial[row] = 0;
        }
    }
}

// A draw is the same as one of the previous frame when it draws the same
// vertices the same way: indexed positions come from the model's array, so
// its address names the mesh; direct ones (2D, particles) are named by their
// first vertex. Repeats of a key pair up in order.
uint64_t fgKey(uint8_t opcode, uint32_t count, uint32_t fmt) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    mix(opcode);
    mix(count);
    mix(fmt);
    const uint32_t posType = (g.cpVcdLo >> 9) & 3;  // VCD_LO: 1 direct, 2-3 indexed
    if (posType >= 2) mix(reinterpret_cast<uintptr_t>(g.arrayBase[0]));
    else if (s_prim.size() >= 12) {
        uint64_t v;
        std::memcpy(&v, s_prim.data(), 8);
        mix(v);
    }
    mix(reinterpret_cast<uintptr_t>(g.texImage[0]));
    return h;
}

void fgNoteDraw(uint8_t opcode, uint32_t count, uint32_t fmt, const VtxFmtLayout& L) {
    FgFrame& f = *s_fgCur;
    FgDraw d{fgKey(opcode, count, fmt), uint32_t(s_cmd.size()), uint32_t(f.rows.size()), 0};
    auto addRow = [&](uint32_t row) {
        if (row > 61) return true;
        for (uint32_t i = d.row0; i < f.rows.size(); i++)
            if (f.rows[i].row == row) return true;
        if (d.nrows == 10) return false;
        FgRow& r = f.rows.emplace_back();
        r.at = s_fgRowSerial[row] == s_fgSerial ? s_fgRowAt[row] : ~0u;
        r.row = row;
        std::memcpy(r.m, &g.xfMem[row * 4], 48);
        d.nrows++;
        return true;
    };
    if (fmt & VF_MTX) {
        const uint8_t* in = s_prim.data();
        for (uint32_t v = 0; v < count && addRow(in[L.mtx]); v++, in += s_primStride) {
        }
    } else {
        addRow(g.cpMatA & 0x3F);
    }
    f.draws.push_back(d);
}

// After sab_process: the chunk itself becomes the recording (s_cmd takes a
// spare buffer instead of being copied).
void fgTakeStream() {
    s_fgSerial++;  // offsets into s_cmd end with it
    FgFrame& f = *s_fgCur;
    if (!s_inFrame) {
        if (s_fgPendingDraw < f.draws.size()) {
            f.rows.resize(f.draws[s_fgPendingDraw].row0);
            f.draws.resize(s_fgPendingDraw);
        }
        s_fgMetrics.clear();
        return;
    }
    std::vector<uint8_t> chunk;
    if (!s_fgPool.empty()) {
        chunk = std::move(s_fgPool.back());
        s_fgPool.pop_back();
    }
    chunk.swap(s_cmd);
    for (size_t at : s_fgMetrics)
        if (at < chunk.size()) chunk[at] = 0;  // no counting in the replay
    s_fgMetrics.clear();
    FgOp op{FgOp::Stream};
    op.buffer = uint32_t(f.buffers.size());
    op.draw0 = s_fgPendingDraw;
    op.draw1 = uint32_t(f.draws.size());
    s_fgPendingDraw = op.draw1;
    f.buffers.push_back(std::move(chunk));
    f.ops.push_back(op);
}

void fgNoteCopy(FgOp::Kind kind, void* dest, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t dw,
                uint16_t dh, uint32_t fmt, bool clear) {
    if (!fgEnabled() || !s_inFrame) return;
    FgOp op{kind};
    op.dest = dest;
    op.x = x, op.y = y, op.w = w, op.h = h, op.dw = dw, op.dh = dh;
    op.fmt = fmt;
    op.clear = clear;
    if (kind == FgOp::CopyDisplay) {
        s_fgCur->shown = true;
        s_fgCur->xfb = dest;
    }
    s_fgCur->ops.push_back(op);
}

void fgNoteDataPass(bool begin, uint16_t w = 0, uint16_t h = 0) {
    if (!fgEnabled() || !s_inFrame) return;
    FgOp op{begin ? FgOp::DataPassBegin : FgOp::DataPassEnd};
    op.w = w, op.h = h;
    s_fgCur->ops.push_back(op);
}

// The matrix half a step on: m + a (m - previous). Not across a jump (a camera
// cut, a teleport, a reused key): then the current matrix.
bool fgExtrapolate(const float* cur, const float* prev, float a, float* out) {
    float rot = 0, tr = 0;
    for (int i = 0; i < 12; i++) {
        const float d = std::fabs(cur[i] - prev[i]);
        if (i % 4 == 3) tr = std::max(tr, d);
        else rot = std::max(rot, d);
    }
    if (rot > 0.3f || tr > 400.f) return false;
    bool moved = false;
    for (int i = 0; i < 12; i++) {
        out[i] = cur[i] + a * (cur[i] - prev[i]);
        moved |= out[i] != cur[i];
    }
    return moved;
}

void fgReplay() {
    const double t0 = nowSeconds();
    static const float ahead = [] {
        const char* v = getenv("SMS_FRAMEGEN_AHEAD");
        return v ? float(atof(v)) : 0.5f;
    }();
    FgFrame& cur = *s_fgCur;
    const FgFrame& prev = *s_fgPrev;
    // Dumps read the replayed image back: that needs the synchronous path.
    static const bool sync = getenv("SMS_FRAMEGEN_DUMP") || getenv("SMS_FRAMEGEN_SYNC");
    if (sync && !sab_begin_frame()) return;
    static std::vector<SabReplayOp> queued;
    queued.clear();
    auto queue = [&](int kind, const FgOp& op) {
        SabReplayOp r{};
        r.kind = kind;
        if (kind == SAB_REPLAY_STREAM) {
            r.bytes = cur.buffers[op.buffer].data();
            r.size = uint32_t(cur.buffers[op.buffer].size());
        }
        r.dest = op.dest;
        r.x = op.x, r.y = op.y, r.w = op.w, r.h = op.h, r.dw = op.dw, r.dh = op.dh;
        r.fmt = op.fmt;
        r.clear = op.clear ? 1 : 0;
        queued.push_back(r);
    };
    double tStep = nowSeconds();
    // The game draws in nearly the same order every frame: each draw is looked
    // for just after the previous match, a few draws on at most (objects that
    // appeared or went away); repeats of a mesh pair up in order.
    constexpr size_t kWindow = 64;
    size_t cursor = 0;
    for (const auto& op : cur.ops) {
        switch (op.kind) {
        case FgOp::Stream: {
            tStep = nowSeconds();
            std::vector<uint8_t>& bytes = cur.buffers[op.buffer];
            // The game's own matrix loads are rewritten in place: the stream
            // keeps its shape, so Aurora merges the draws as it did.
            for (uint32_t di = op.draw0; di < op.draw1; di++) {
                const FgDraw& d = cur.draws[di];
                s_fgDrawsSeen++;
                const FgDraw* p = nullptr;
                const size_t end = std::min(prev.draws.size(), cursor + kWindow);
                for (size_t k = cursor; k < end; k++)
                    if (prev.draws[k].key == d.key) {
                        p = &prev.draws[k];
                        cursor = k + 1;
                        break;
                    }
                if (!p || p->nrows != d.nrows) continue;
                s_fgMatched++;
                for (uint32_t i = 0; i < d.nrows; i++) {
                    const FgRow& r = cur.rows[d.row0 + i];
                    if (r.at == ~0u || r.at + 48 > bytes.size()) continue;
                    // Only where the load still holds the matrix the draw used
                    // (an earlier draw sharing the load has moved it already).
                    if (std::memcmp(&bytes[r.at], r.m, 48) != 0) {
                        s_fgStale++;
                        continue;
                    }
                    float m[12];
                    if (!fgExtrapolate(r.m, prev.rows[p->row0 + i].m, ahead, m)) continue;
                    std::memcpy(&bytes[r.at], m, 48);
                    s_fgRows++;
                }
            }
            const double tb = nowSeconds();
            s_fgBuild += tb - tStep;
            if (sync) sab_process(bytes.data(), uint32_t(bytes.size()));
            else queue(SAB_REPLAY_STREAM, op);
            tStep = nowSeconds();
            s_fgProcess += tStep - tb;
            break;
        }
        case FgOp::CopyTex:
            if (sync) sab_copy_tex_request(op.dest, op.x, op.y, op.w, op.h, op.dw, op.dh, op.fmt, op.clear ? 1 : 0, 0, 0);
            else queue(SAB_REPLAY_COPY_TEX, op);
            break;
        case FgOp::CopyDisplay: {
            if (!sync) {
                queue(SAB_REPLAY_COPY_DISPLAY, op);
                break;
            }
            static const bool dump = getenv("SMS_FRAMEGEN_DUMP") != nullptr;
            static const uint32_t dumpFrom = getenv("SMS_GX_DUMP_FROM") ? uint32_t(atoi(getenv("SMS_GX_DUMP_FROM"))) : 0;
            if (dump && dumpDir() && s_frame >= dumpFrom && s_frame % uint32_t(dumpEvery()) == 0) {
                static const std::string dir = std::string(dumpDir()) + "/fg";
                static const bool made = (_mkdir(dir.c_str()), true);
                (void)made;
                void* marker = &s_fgMarkers[s_fgMarkerNext++ % sizeof s_fgMarkers];
                if (!s_dumpPending.count(marker)) {
                    ensureCopyReadbackCapacity();
                    const uint64_t requestId = ++s_nextCopyRequest;
                    s_dumpPending[marker] = PendingDump{s_frame, dir, requestId};
                    sab_copy_tex_request(marker, op.x, op.y, op.w, op.h, op.w, op.h, GX_TF_RGBA8, 0, 1, requestId);
                }
            }
            sab_copy_display(op.dest, op.x, op.y, op.w, op.h, op.clear ? 1 : 0);
            break;
        }
        case FgOp::DataPassBegin:
            if (sync) sab_begin_data_pass(op.w, op.h);
            else queue(SAB_REPLAY_DATA_PASS_BEGIN, op);
            break;
        case FgOp::DataPassEnd:
            if (sync) sab_end_data_pass();
            else queue(SAB_REPLAY_DATA_PASS_END, op);
            break;
        }
    }
    // Half a game frame after the game's own image.
    const double wait = s_fgLastLive + s_fgPeriod * 0.5 - nowSeconds();
    const uint32_t waitUs = wait > 0 ? uint32_t(wait * 1e6) : 0;
    const double tf = nowSeconds();
    if (sync) {
        sab_select_scanout(cur.xfb, 0);
        sab_present_after(waitUs);
        sab_finish_frame(1);
    } else {
        // The bridge reads the chunks where they are: they stay untouched
        // until this frame's buffers are reused, two frames from now, after
        // the GX thread has long finished with them.
        sab_submit_replay(queued.data(), uint32_t(queued.size()), cur.xfb, waitUs);
        s_fgAsyncBegin = true;
    }
    s_fgFinish += nowSeconds() - tf;
    s_fgReplays++;
    s_fgSeconds += nowSeconds() - t0;
}

// Frame generation steps aside when the game stops keeping up: the replay
// costs the GX thread about as much as the game's frame, and in a heavy scene
// the two together can make the game miss retraces. Blocks of 60 game frames
// are counted; 4 misses or more in one pause the replays, for 2 seconds at
// first, twice as long each time it happens again soon after (16 s at most),
// back to 2 seconds after a clean block.
struct FgPacing {
    uint32_t frames = 0, misses = 0;
    uint32_t pausedUntil = 0;  // s_frame
    uint32_t pause = 120;
    bool paused(uint32_t frame) const { return frame < pausedUntil; }
} s_fgPacing;

void fgCountFrame(double dt) {
    FgPacing& p = s_fgPacing;
    if (p.paused(s_frame)) return;
    const bool fps60 = &port_fps60_active && port_fps60_active;
    const double expected = fps60 ? 1.0 / 59.94 : 2.0 / 59.94;
    p.frames++;
    p.misses += dt > expected * 1.5 && dt < 0.25;  // not a load or a pause
    if (p.frames < 60) return;
    if (p.misses >= 4) {
        p.pausedUntil = s_frame + p.pause;
        fprintf(stderr, "[gx-aurora] framegen: %u missed retraces in 60 frames, paused for %u frames\n", p.misses,
                p.pause);
        p.pause = std::min<uint32_t>(p.pause * 2, 960);
    } else if (p.misses == 0) {
        p.pause = 120;
    }
    p.frames = p.misses = 0;
}

// After the game's frame is presented: play it again, half a step on.
void fgAfterPresent(bool shown) {
    if (!fgEnabled()) return;
    if (shown && s_fgCur->shown) {
        const double now = nowSeconds();
        if (s_fgLastLive > 0) {
            const double dt = now - s_fgLastLive;
            if (dt > 0.005 && dt < 0.05) s_fgPeriod = 0.9 * s_fgPeriod + 0.1 * dt;
            fgCountFrame(dt);
        }
        s_fgLastLive = now;
        if (s_fgCur->valid && !s_fgPrev->ops.empty() && s_fgCur->xfb && !s_fgPacing.paused(s_frame)) fgReplay();
        std::swap(s_fgCur, s_fgPrev);
    }
    s_fgCur->clear();
    s_fgPendingDraw = 0;
    s_fgSerial++;
    s_fgMetrics.clear();
}

// ------------------------------------------------------------------ frames
void beginFrame(bool retain = true) {
    const double t0 = nowSeconds();
    const bool async = s_fgAsyncBegin;
    s_fgAsyncBegin = false;
    if (async) {
        sab_begin_frame_async(retain ? 1 : 0);
        s_inFrame = true;
    } else {
        s_inFrame = sab_begin_frame() != 0;
        if (s_inFrame && retain) sab_retain_efb();
    }
    const double waited = nowSeconds() - t0;
    s_acquireLast = waited;
    s_acquireAccum += waited;
    s_acquireLong += waited > 0.004;
    s_frameIdKnown = !async;
    if (!async) s_frameId = s_inFrame ? sab_frame_id() : 0;
    // Aurora starts each frame with fresh state: send the vertex format again
    // and every texture before its next use.
    s_curFmt = ~0u;
    for (auto& m : s_maps) m.regsDirty = true;
    for (bool& v : s_suValid) v = false;
    if (!async) storeCopyReadbacks();
}

void reportStats() {
    if (!s_statsEvery) {
        s_submitSeconds=0; s_acquireAccum=0; s_acquireLong=0;
        s_writeBackCount=0; s_presentAccum=0; s_stats={};
        return;
    }
    if (s_frameTimes.size() < size_t(s_statsEvery)) return;
    std::vector<double> t = s_frameTimes;
    std::sort(t.begin(), t.end());
    double med = t[t.size() / 2] * 1000, p90 = t[t.size() * 9 / 10] * 1000, worst = t.back() * 1000;
    double n = double(s_frameTimes.size());
    // Hitches: frames well over the target (twice the median, and at least 25 ms).
    int hitches = 0;
    for (double f : t) hitches += f * 1000 > std::max(25.0, med * 1.8);
    static const double start = nowSeconds();
    // The VI wait (time the game idles until the next retrace): what is left once the frame's work is done.
    static double lastIdle = 0;
    const double idleNow = s_idleClock ? s_idleClock() : 0, idle = idleNow - lastIdle;
    lastIdle = idleNow;
    fprintf(stderr,
            "[gx-aurora] t=%.0fs %s: frame median %.1f ms, p90 %.1f ms, worst %.1f ms, %d hitches; per frame: "
            "VI idle %.2f ms, Aurora submit %.2f ms, present %.2f ms, acquire %.2f ms (%d over 4 ms), %.0f draws, "
            "%.0f vertices, %.1f texture loads, %.1f rehashes, %.1f copies\n",
            nowSeconds() - start, sab_backend_name(), med, p90, worst, hitches, idle / n * 1000,
            s_submitSeconds / n * 1000, s_presentAccum / n * 1000, s_acquireAccum / n * 1000, s_acquireLong,
            s_stats.draws / n, s_stats.vertices / n, s_stats.texLoads / n, s_stats.texRehash / n,
            s_stats.copies / n);
    s_submitSeconds = 0;
    {
        double busy = 0, waited = 0;
        sab_gx_thread_times(&busy, &waited);
        fprintf(stderr, "[gx-aurora] GX thread: busy %.2f ms, game waited %.2f ms per frame\n", busy / n * 1000,
                waited / n * 1000);
        if (fgEnabled()) {
            double b[5];
            sab_gx_thread_breakdown(b);
            fprintf(stderr, "[gx-aurora] GX thread replay: begin %.2f, stream %.2f, copies %.2f, end %.2f; next begin %.2f ms per frame\n",
                    b[0] / n * 1000, b[1] / n * 1000, b[2] / n * 1000, b[3] / n * 1000, b[4] / n * 1000);
        }
    }
    if (fgEnabled()) {
        fprintf(stderr, "[gx-aurora] framegen: %u replays, %u of %u draws matched, %u rows (%u stale), %.2f ms per replay"
                " (build %.2f, process %.2f, finish %.2f)\n", s_fgReplays, s_fgMatched, s_fgDrawsSeen, s_fgRows, s_fgStale,
                s_fgReplays ? s_fgSeconds / s_fgReplays * 1000 : 0.0, s_fgReplays ? s_fgBuild / s_fgReplays * 1000 : 0.0,
                s_fgReplays ? s_fgProcess / s_fgReplays * 1000 : 0.0, s_fgReplays ? s_fgFinish / s_fgReplays * 1000 : 0.0);
        s_fgReplays = s_fgMatched = s_fgDrawsSeen = s_fgRows = s_fgStale = 0;
        s_fgSeconds = s_fgBuild = s_fgProcess = s_fgFinish = 0;
    }
    s_acquireAccum = 0;
    s_acquireLong = 0;
    if (!s_tokenValues.empty() || s_writeBackCount) {
        fprintf(stderr, "[gx-aurora] copy write-backs %u; pixel metrics:", s_writeBackCount);
        for (const auto& [token, value] : s_tokenValues) fprintf(stderr, " %u=%u", unsigned(token), value);
        fprintf(stderr, "\n");
        s_writeBackCount = 0;
    }
    s_frameTimes.clear();
    s_presentAccum = 0;
    s_stats = Stats();
}

void pumpEvents();

// Auto frame rate (settings `frame_rate = 60`): when 60 fps gameplay keeps
// missing retraces (a PC on battery, or too slow), the game runs slowed down,
// since it moves one step per frame. It drops to 30 instead, and goes back to
// 60 once its work per frame fits again: 12 misses in 120 frames drop it; at
// 30, 9 frames in 10 under 13 ms of work over 150 frames bring 60 back. A PC
// at the edge would swing between the two: a drop within 20 s of going back
// doubles the frames measured before the next try (600 at most).
// SMS_AUTO_FRAME_RATE=0 turns it off.
void autoFrameRate(double dt) {
    static const bool on = [] { const char* v = getenv("SMS_AUTO_FRAME_RATE"); return !v || strcmp(v, "0"); }();
    if (!on || !&port_frame_rate || !&port_frame_rate_auto_capped || !&port_fps60_active ||
        !sms_frontend_sync_game_rate || !s_idleClock)
        return;
    static double lastIdle = 0;
    const double idleNow = s_idleClock();
    const double work = dt - (idleNow - lastIdle);
    lastIdle = idleNow;
    static uint32_t frames = 0, misses = 0;
    static size_t needed = 150;
    static double backAt = -1e9;
    static std::vector<double> works;
    if (port_fps60_active) {
        frames++;
        misses += dt > 0.025 && dt < 0.25;  // not a load or a pause
        if (frames < 120) return;
        if (misses >= 12) {
            needed = nowSeconds() - backAt < 20 ? std::min<size_t>(needed * 2, 600) : 150;
            port_frame_rate = 30;
            port_frame_rate_auto_capped = 1;
            sms_frontend_sync_game_rate();
            works.clear();
            fprintf(stderr, "[gx-aurora] auto frame rate: %u missed retraces in 120 frames, 30 fps\n", misses);
        }
        frames = misses = 0;
    } else if (port_frame_rate_auto_capped && dt < 0.25) {
        works.push_back(work);
        if (works.size() < needed) return;
        std::sort(works.begin(), works.end());
        const double p90 = works[works.size() * 9 / 10];
        works.clear();
        if (p90 < 0.013) {
            port_frame_rate = 60;
            port_frame_rate_auto_capped = 0;
            sms_frontend_sync_game_rate();
            frames = misses = 0;
            backAt = nowSeconds();
            fprintf(stderr, "[gx-aurora] auto frame rate: work %.1f ms a frame (9 in 10), back to 60 fps\n", p90 * 1000);
        }
    }
}

// frame, ms since the previous present, then within that interval: the game's
// VI idle, Aurora submit (draw calls), the end-of-frame work, the swapchain wait
// at the previous frame's start, the retraces that passed and the VI timer's
// worst lateness.
void writeFrameLog(double t0, double t1) {
    if (port_os_profile_frame) port_os_profile_frame(s_frame);
    static double lastIdle = 0, lastSubmit = 0;
    static uint32_t lastRetrace = 0;
    const double idle = s_idleClock ? s_idleClock() : 0;
    const uint32_t retrace = VIGetRetraceCount();
    const double late = port_vi_take_late_max ? port_vi_take_late_max() : 0;
    if (s_lastFrameEnd > 0)
        fprintf(s_frameLog, "%u %.2f idle %.2f submit %.2f end %.2f acquire %.2f retraces %u late %.2f\n", s_frame,
                (t1 - s_lastFrameEnd) * 1000, (idle - lastIdle) * 1000, (s_submitTotalSeconds - lastSubmit) * 1000,
                (t1 - t0) * 1000, s_acquireLast * 1000, retrace - lastRetrace, late * 1000);
    lastIdle = idle;
    lastSubmit = s_submitTotalSeconds;
    lastRetrace = retrace;
}

// VI chooses the visible XFB independently of the buffer GX is writing.
bool prepareScanout(const void* explicitXfb = nullptr, bool force = false) {
    if (!s_inFrame || (!s_autoPresent && !force)) return false;
    const void* xfb = explicitXfb ? explicitXfb : s_lastCopiedXfb;
    int viBlack = 0;
    if (!explicitXfb && s_viScanout && !s_viScanout(&xfb, &viBlack)) return false;
    const auto it = s_xfbSerials.find(xfb);
    const uint64_t serial = it == s_xfbSerials.end() ? 0 : it->second;
    bool black = viBlack != 0 || !xfb || !serial;
    uint32_t width = 0, height = 0;
    sab_get_window_size(&width, &height);
    bool changed = force || !s_haveScanout || black != s_presentedBlack ||
        (!black && (xfb != s_presentedXfb || serial != s_presentedSerial)) ||
        width != s_presentedWidth || height != s_presentedHeight;
#ifdef SMS_PARTYBOARD_UI
    changed = changed || GXPC_MenuVisible();
#endif
    if (!changed) return false;
    if (!sab_select_scanout(xfb, black ? 1 : 0)) black = true;
    s_presentedXfb = xfb; s_presentedSerial = serial; s_presentedBlack = black;
    s_presentedWidth = width; s_presentedHeight = height; s_haveScanout = true;
    ++s_presentedCount;
    static const bool log = getenv("SMS_VI_LOG") != nullptr;
    static unsigned logged = 0;
    if (log && logged++ < 120)
        fprintf(stderr,"[gx-aurora] VI present %llu at copy %u: XFB %p serial %llu black %d\n",
                (unsigned long long)s_presentedCount,s_frame,xfb,(unsigned long long)serial,int(black));
    return true;
}

void present(bool show = true) {
    double t0 = nowSeconds();
    endFrameMetrics();
    submit();
    const bool wasInFrame = s_inFrame;
    if (s_inFrame) sab_finish_frame(show ? 1 : 0);
    s_inFrame = false;
    collectPixelMetrics();
    storeCopyReadbacks();
    const bool fgShown = show && wasInFrame;
    double t1 = nowSeconds();
    if (show) {
        s_presentAccum += t1 - t0;
        if (s_statsEvery && s_lastFrameEnd > 0) s_frameTimes.push_back(t1 - s_lastFrameEnd);
        if (s_frameLog) writeFrameLog(t0, t1);
        if (s_profileLog) {
            fprintf(s_profileLog, "%u %.3f draws %u draw %.3f textures %.3f peeks %u peek %.3f\n",
                    s_frame, s_lastFrameEnd > 0 ? (t1-s_lastFrameEnd)*1000 : 0,
                    s_profile.draws, s_profile.draw*1000, s_profile.textures*1000,
                    s_profile.peeks, s_profile.peek*1000);
            s_profile = {};
        }
        if (s_lastFrameEnd > 0) autoFrameRate(t1 - s_lastFrameEnd);
        s_lastFrameEnd = t1;
        reportStats();
    }
    pumpEvents();
#ifdef SMS_PARTYBOARD_UI
    // Keep presenting the retained game image and WebGPU UI while the game
    // thread is paused. No game update, GX metric or CARD command is issued.
    while (s_ready && GXPC_MenuVisible()) {
        if (sab_begin_menu_frame()) sab_end_frame();
        collectPixelMetrics();
        storeCopyReadbacks();
        pumpEvents();
        sab_delay(8);
    }
#endif
    // Bounded diagnostic runs exit normally, including renderer shutdown.
    static const unsigned stopAfter = [] {
        const char* value = getenv("SMS_GX_STOP_AFTER");
        return value ? unsigned(strtoul(value, nullptr, 10)) : 0u;
    }();
    if (stopAfter && s_frame >= stopAfter) std::exit(0);
    fgAfterPresent(fgShown);
    beginFrame();
}

// ------------------------------------------------------------------ input
namespace sdl2 {
// SDL2's event ABI, which platform/pad reads (see pad.cpp).
struct KeyboardEvent {
    uint32_t type, timestamp, windowID;
    uint8_t state, repeat, pad2, pad3;
    int32_t scancode, sym;
    uint16_t mod;
    uint32_t unused;
};
struct MouseMotionEvent {
    uint32_t type, timestamp, windowID, which, state;
    int32_t x, y, xrel, yrel;
};
struct ControllerAxisEvent {
    uint32_t type, timestamp;
    int32_t which;
    uint8_t axis, p1, p2, p3;
    int16_t value;
    uint16_t p4;
};
struct ControllerButtonEvent {
    uint32_t type, timestamp;
    int32_t which;
    uint8_t button, state, p1, p2;
};
union Event {
    uint32_t type;
    KeyboardEvent key;
    MouseMotionEvent motion;
    ControllerAxisEvent caxis;
    ControllerButtonEvent cbutton;
    uint8_t padding[56];
};
}  // namespace sdl2

enum { SC_RETURN = 40, SC_GRAVE = 53, SC_F7 = 64, SC_F10 = 67, SC_F11 = 68, SC_KP_ENTER = 88, KMOD_ALT = 0x0300 };

void captureMouse(bool on) {
    if (on == s_mouseCaptured) return;
    s_mouseCaptured = on;
    sab_set_mouse_relative(on ? 1 : 0);
}

void pumpEvents() {
    if (!s_ready) return;
    SabEvent ev;
    while (sab_poll_event(&ev)) {
        sdl2::Event out;
        memset(&out, 0, sizeof out);
        out.type = ev.type;
        switch (ev.type) {
        case SAB_EV_QUIT:
            fprintf(stderr, "[gx-aurora] window closed, exiting\n");
            fflush(stderr);
            GXPC_Shutdown();
            exit(0);
        case SAB_EV_FOCUS_LOST:
            s_hasFocus = false;
            if (s_mouseCamera) captureMouse(false);
            continue;
        case SAB_EV_FOCUS_GAINED:
            s_hasFocus = true;
            s_haveScanout = false;
            if (s_mouseCamera && !s_mouseReleased) captureMouse(true);
            continue;
        case SAB_EV_MOUSEBUTTONDOWN:
        case SAB_EV_MOUSEBUTTONUP:
            // With mouse look, a click on the window first captures the mouse.
            if (ev.type == SAB_EV_MOUSEBUTTONDOWN && s_mouseCamera && !s_mouseCaptured) {
                s_mouseReleased = false;
                captureMouse(true);
                continue;
            }
            // Left, middle and right buttons reach the pad as keys 505-507
            // (MOUSE_LEFT, MOUSE_MIDDLE, MOUSE_RIGHT in bindings.txt).
            if (ev.button < 1 || ev.button > 3) continue;
            out.type = ev.type == SAB_EV_MOUSEBUTTONDOWN ? SAB_EV_KEYDOWN : SAB_EV_KEYUP;
            out.key.state = ev.type == SAB_EV_MOUSEBUTTONDOWN ? 1 : 0;
            out.key.scancode = 504 + ev.button;
            break;
        case SAB_EV_KEYDOWN:
        case SAB_EV_KEYUP: {
            const bool down = ev.type == SAB_EV_KEYDOWN;
            const bool fresh = down && !ev.repeat;
            if (ev.scancode == SC_F11 ||
                ((ev.scancode == SC_RETURN || ev.scancode == SC_KP_ENTER) && (ev.mod & KMOD_ALT))) {
                if (fresh) sab_set_fullscreen(!sab_is_fullscreen());
                continue;
            }
            if (ev.scancode == SC_F10 && s_mouseCamera) {
                if (fresh) {
                    s_mouseReleased = s_mouseCaptured;
                    captureMouse(!s_mouseCaptured);
                }
                continue;
            }
            if (ev.scancode == SC_F7) {
                // Keep simulation and audio at the original speed.
                continue;
            }
            if (ev.scancode == SC_GRAVE) continue;
            if (ev.scancode == 66) {  // F9: capture what is on screen for analysis
                if (fresh) {
                    time_t now = time(nullptr);
                    char stamp[32];
                    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", localtime(&now));
                    _mkdir("artifacts");
                    _mkdir("artifacts/aurora-captures");
                    s_captureDir = std::string("artifacts/aurora-captures/") + stamp;
                    _mkdir(s_captureDir.c_str());
                    s_captureUntil = s_frame + 60;
                    fprintf(stderr, "[gx-aurora] capture F9 at frame %u -> %s\n", s_frame, s_captureDir.c_str());
                    fflush(stderr);
                }
                continue;
            }
            out.key.state = down ? 1 : 0;
            out.key.repeat = ev.repeat;
            out.key.scancode = ev.scancode;
            out.key.mod = ev.mod;
            break;
        }
        case SAB_EV_MOUSEMOTION:
            out.motion.xrel = int32_t(ev.xrel);
            out.motion.yrel = int32_t(ev.yrel);
            break;
        case SAB_EV_PADAXIS:
            out.caxis.which = ev.which;
            out.caxis.axis = ev.button;
            out.caxis.value = int16_t(ev.value);
            break;
        case SAB_EV_PADBUTTONDOWN:
        case SAB_EV_PADBUTTONUP:
            out.cbutton.which = ev.which;
            out.cbutton.button = ev.button;
            out.cbutton.state = ev.type == SAB_EV_PADBUTTONDOWN ? 1 : 0;
            break;
        default:
            continue;
        }
        if (s_eventCb) s_eventCb(reinterpret_cast<const union SDL_Event*>(&out));
    }
#ifdef SMS_PARTYBOARD_UI
    const bool wantMouse = s_hasFocus && s_mouseCamera && !s_mouseReleased && !GXPC_MenuVisible();
    captureMouse(wantMouse);
#endif
}

bool envTrue(const char* name) {
    const char* e = getenv(name);
    return e && *e && strcmp(e, "0") != 0;
}

int backendFromEnv() {
    const char* e = getenv("SMS_AURORA_BACKEND");
    if (!e || !*e) return 2;  // Direct3D 12
    if (!strcmp(e, "auto")) return 0;
    if (!strcmp(e, "d3d11")) return 1;
    if (!strcmp(e, "d3d12")) return 2;
    if (!strcmp(e, "vulkan")) return 4;
    if (!strcmp(e, "opengl")) return 5;
    return 2;
}

}  // namespace

// ------------------------------------------------------------------ register sink (gx_fifo.cpp)
void sinkBP(uint32_t reg, uint32_t data) {
    switch (reg) {
    // handled here or in the front end, never by Aurora
    case BP_MASK:
    case BP_DRAWDONE:
    case BP_PE_TOKEN:
    case BP_PE_TOKEN_INT:
    case BP_COPY_SRC_TL:
    case BP_COPY_SRC_WH:
    case BP_COPY_DST:
    case BP_COPY_STRIDE:
    case BP_COPY_YSCALE:
    case BP_COPY_EXEC:
    case BP_LOADTLUT0:
    case BP_LOADTLUT1:
    case BP_TEXINVALIDATE:
        return;
    default:
        break;
    }
    if ((reg >= BP_TX_IMAGE3 && reg < BP_TX_IMAGE3 + 4) || (reg >= BP_TX_IMAGE3 + 0x20 && reg < BP_TX_IMAGE3 + 0x24))
        return;  // host pointers travel in GX_LOAD_AURORA_TEXOBJ
    if ((reg >= BP_TX_IMAGE0 && reg < BP_TX_IMAGE0 + 4) || (reg >= BP_TX_IMAGE0 + 0x20 && reg < BP_TX_IMAGE0 + 0x24))
        // sinkBP runs before the front-end shadow is updated. Aurora's BP
        // handler ignores identical writes, so those do not forget the size.
        if(g.bp[reg]!=data) s_maps[(reg & 3) + ((reg & 0x20) ? 4 : 0)].regsDirty = true;
    if (reg >= BP_SU_SSIZE && reg < BP_SU_SSIZE + 16) s_suValid[reg - BP_SU_SSIZE] = false;
    static const char* dbg = getenv("SMS_AURORA_XFDBG");  // c = log TEV colour registers
    if (dbg && strchr(dbg, 'c') && reg >= 0xE0 && reg <= 0xE7) {
        static std::map<uint32_t, int> seen;
        if (seen[reg << 24 | data]++ == 0) fprintf(stderr, "[gx-aurora] bp %02x = %06x\n", reg, data);
    }
    putBP(reg, data);
}

void sinkCP(uint8_t reg, uint32_t value) {
    // The vertex loader runs in gx_fifo.cpp: only the matrix indices matter.
    if ((reg & 0xF0) == CP_MATIDX_A || (reg & 0xF0) == CP_MATIDX_B) {
        static uint32_t sent[2]{};
        static bool valid[2]{};
        const unsigned slot = (reg & 0xF0) == CP_MATIDX_B;
        if (stateCacheEnabled() && valid[slot] && sent[slot] == value) return;
        sent[slot] = value;
        valid[slot] = true;
        putCP(reg, value);
    }
}

void sinkXF(uint16_t addr, uint32_t count, const uint32_t* words) {
    // Cache only XF memory (matrices/lights). Viewport/projection registers
    // have command-size-dependent decoding and temporary per-draw overrides.
    // Preserve every register command and the original span of changed loads.
    static uint32_t sent[0x1000]{};
    static bool valid[0x1000]{};
    static const char* debug = getenv("SMS_AURORA_XFDBG");
    if (stateCacheEnabled() && !debug && addr < 0x1000 && count <= 0x1000u - addr) {
        if (std::memcmp(sent + addr, words, count * sizeof(uint32_t)) == 0 &&
            std::all_of(valid + addr, valid + addr + count, [](bool v) { return v; })) return;
        std::memcpy(sent + addr, words, count * sizeof(uint32_t));
        std::fill(valid + addr, valid + addr + count, true);
    }
    put8(0x10);
    put32((count - 1) << 16 | addr);
    size_t n = s_cmd.size();
    s_cmd.resize(n + size_t(count) * 4);
    memcpy(&s_cmd[n], words, size_t(count) * 4);
    if (fgEnabled() && s_inFrame) fgNoteXf(addr, count, n);
    // SMS_AURORA_XFDBG: l = lighting off, t = log texgens and channel controls, L = log lights
    static const char* dbg = getenv("SMS_AURORA_XFDBG");
    if (dbg) {
        uint32_t* w = reinterpret_cast<uint32_t*>(&s_cmd[n]);
        for (uint32_t i = 0; i < count; i++) {
            uint32_t a = addr + i;
            if (strchr(dbg, 'l') && a >= 0x100E && a <= 0x1011) w[i] &= ~2u;  // lighting off
            if (strchr(dbg, 'L') && a >= 0x600 && a < 0x680 && i == 0) {
                static int n = 0;
                if (n++ < 60) {
                    fprintf(stderr, "[gx-aurora] light write %04x x%u:", a, count);
                    for (uint32_t k = 0; k < count; k++) {
                        float f;
                        memcpy(&f, &w[k], 4);
                        if ((a + k - 0x600) % 16 == 3) fprintf(stderr, " c=%08x", w[k]);
                        else fprintf(stderr, " %g", f);
                    }
                    fprintf(stderr, "\n");
                }
            }
            if (strchr(dbg, 't') && ((a >= 0x1040 && a <= 0x1047) || (a >= 0x100E && a <= 0x1011))) {
                static std::map<uint64_t, int> seen;
                if (seen[uint64_t(a) << 32 | w[i]]++ == 0)
                    fprintf(stderr, "[gx-aurora] xf %04x = %08x\n", a, w[i]);
            }
        }
    }
}

// ------------------------------------------------------------------ renderer interface (gx_internal.h)
bool rendererReady() { return s_ready; }
void flushBatch() {}
void onStateChange() {}
void markXfMemDirty() {}
void finishDraws() { submit(); }
void synchronizeReadbacks() {
    if (!s_ready || s_pendingCopies.empty()) return;
    storeCopyReadbacks();
}

uint8_t* primitiveBegin(uint8_t, uint32_t count, uint32_t fmt, uint32_t stride) {
    s_primFmt = fmt;
    s_primStride = stride;
    size_t need = size_t(count) * stride;
    if (s_prim.size() < need) s_prim.resize(need);
    return s_prim.data();
}

void primitiveEnd(uint8_t opcode, uint32_t count) {
    if (!count) return;
    // Aurora draws points and lines itself; quads, strips and fans as given.
    emitDraw(opcode, count);
}

void executeCopy(uint32_t ctrl) {
    if (!s_ready) return;
    metricStop();
    submit();
    uint32_t src = g.bp[BP_COPY_SRC_TL], size = g.bp[BP_COPY_SRC_WH];
    uint16_t x = uint16_t(src & 0x3FF), y = uint16_t((src >> 10) & 0x3FF);
    uint16_t w = uint16_t((size & 0x3FF) + 1), h = uint16_t(((size >> 10) & 0x3FF) + 1);
    const bool disp = (ctrl >> 14) & 1;
    const bool clear = (ctrl >> 11) & 1;
    void* dest = g.copyDest;
    if (dest && !(ctrl & (1u << 14))) {
        s_efbCopyDestinations.insert(dest);
        s_copyVersions[dest]++;
        for (auto& ms : s_maps) {
            if (ms.data == dest) ms.regsDirty = true;
        }
    }
    if (disp) {
        if (s_dataPassActive) { sab_end_data_pass(); s_dataPassActive = false; fgNoteDataPass(false); }
        const bool sizeChanged = w != s_efbW || h != s_efbH;
        s_frame++;
        static const uint32_t dumpFrom = getenv("SMS_GX_DUMP_FROM") ? uint32_t(atoi(getenv("SMS_GX_DUMP_FROM"))) : 0;
        const bool dumpThis = dumpDir() && s_frame >= dumpFrom && s_frame % uint32_t(dumpEvery()) == 0;
        const bool captureThis = s_frame <= s_captureUntil && s_frame % 2 == 0;
        if (s_inFrame && (dumpThis || captureThis)) {
            void* marker = &s_dumpMarkers[s_dumpNext++ % sizeof s_dumpMarkers];
            if (!s_dumpPending.count(marker)) {
                const uint64_t requestId = ++s_nextCopyRequest;
                s_dumpPending[marker] = PendingDump{s_frame, captureThis ? s_captureDir : std::string(dumpDir()), requestId};
                sab_copy_tex_request(marker, x, y, w, h, w, h, GX_TF_RGBA8, 0, 1, requestId);
            }
        }
        if (g_displayCopyHook) g_displayCopyHook(dest);
        fgNoteCopy(FgOp::CopyDisplay, dest, x, y, w, h, w, h, 0, clear);
        if (s_inFrame && sab_copy_display(dest,x,y,w,h,clear ? 1 : 0)) {
            s_xfbSerials[dest] = ++s_copySerial;
            s_lastCopiedXfb = dest;
        }
        // The completed image was drawn using the old logical EFB mapping.
        // Reconfigure only after capturing it, for subsequent game draws.
        if (sizeChanged) {
            fgInvalidate();
            s_efbW = w; s_efbH = h;
            sab_set_efb_size(w,h);
        }
        const bool show = prepareScanout();
        present(show);
        return;
    }
    const bool half = (ctrl >> 9) & 1;
    uint16_t dw = half ? uint16_t(w / 2) : w, dh = half ? uint16_t(h / 2) : h;
    if (!dw) dw = 1;
    if (!dh) dh = 1;
    // Texture copies are specified in GameCube logical coordinates; Aurora's map_logical_scissor
    // scales them to the render target. Sub-viewport/offscreen texture copies (such as pollution layers
    // and water distortion) must never be squished or shifted by widescreen HUD margins.
    const uint16_t cx = x, cw = w;
    const uint32_t f = ((ctrl >> 3) & 1) << 3 | ((ctrl >> 4) & 7);
    const bool zcopy = (g.bp[BP_PE_CONTROL] & 7) == 3;
    // Only data copies come back to RAM (Delfino's pollution layers are R8 into I8):
    // the colour copies are screen effects the CPU does not read.
    // SMS_GX_COPY_WRITEBACK=all writes every copy back.
    const uint32_t layout = copyLayout(f, false);
    const bool readback = s_inFrame && dest && !zcopy && copyWriteBackEnabled() &&
                          (layout <= 3 || copyWriteBackAll());
    if (readback && !s_dataPassActive) {
        ensureCopyReadbackCapacity();
    }
    uint64_t requestId = 0;
    if (readback) {
        PendingCopy pc{};
        pc.requestId = requestId = ++s_nextCopyRequest;
        pc.dest = dest;
        pc.frameId = curFrameId();
        pc.width = dw;
        pc.height = dh;
        pc.fmt = f;
        pc.layout = copyLayout(f, false);
        pc.bytes = texLevelBytes(pc.layout, dw, dh);
        pc.intensity = ((ctrl >> 15) & 3) == 3;
        pc.alphaOne = (g.bp[BP_PE_CONTROL] & 7) != 1;  // only RGBA6_Z24 keeps alpha
        pc.hash = hashBytes(dest, pc.bytes);
        s_pendingCopies.emplace(requestId, pc);
    }
    static const bool copyLog = getenv("SMS_GX_COPY_LOG") != nullptr;
    if (copyLog) {
        static std::map<std::tuple<const void*, uint32_t, uint32_t, uint32_t>, int> seen;
        if (seen[{dest, uint32_t(dw) << 16 | dh, f, (((ctrl >> 15) & 3) == 3)}]++ == 0)
            fprintf(stderr, "[gx-aurora] copy %p %ux%u (src %ux%u at %u,%u) fmt %u intensity %d layout %u, %u bytes\n",
                    dest, dw, dh, w, h, x, y, f, int((((ctrl >> 15) & 3) == 3)), copyLayout(f, false), texLevelBytes(copyLayout(f, false), dw, dh));
    }
    // The bathtub mesh reads this depth buffer on the CPU in the same frame.
    // Capture before the copy/clear changes the EFB; never enqueue it as a
    // delayed colour write-back. RAM retains GX byte order (PC reader decodes BE).
    if (s_inFrame && dest && g_texCopyFmt == GX_TF_Z24X8) {
        fgInvalidate();
        if (!copyDepthZ24X8(dest, cx, y, cw, h, dw, dh)) {
            static uint32_t reported = 0;
            if (reported++ < 4) fprintf(stderr, "[gx-aurora] exact Z24X8 CPU copy failed at frame %u\n", s_frame);
        }
    }
    fgNoteCopy(FgOp::CopyTex, dest, cx, y, cw, h, dw, dh, g_texCopyFmt, clear);
    if (s_inFrame) sab_copy_tex_request(dest, cx, y, cw, h, dw, dh, g_texCopyFmt, clear ? 1 : 0, readback ? 1 : 0, requestId);
    if (s_dataPassActive) { sab_end_data_pass(); s_dataPassActive = false; fgNoteDataPass(false); }
    s_stats.copies++;
}

// Colour/depth probes share the exact EFB checkpoint until a draw or copy
// changes it. Do not use a final-frame image: the game probes before HUD draws.
static uint64_t s_peekRevision = 0;
static uint64_t s_peekDraws = ~uint64_t(0), s_peekCopies = ~uint64_t(0), s_peekFrame = ~uint64_t(0);
static bool peekPixel(int x, int y, uint32_t& color, uint32_t& z) {
    if (!s_ready || !s_inFrame || x < 0 || y < 0) return false;
    if (s_peekDraws != s_stats.draws || s_peekCopies != s_stats.copies || s_peekFrame != curFrameId()) {
        ++s_peekRevision;
        s_peekDraws = s_stats.draws; s_peekCopies = s_stats.copies; s_peekFrame = curFrameId();
    }
    submit();
    const double start = nowSeconds();
    const bool valid = sab_peek_efb(uint32_t(x), uint32_t(y), s_peekRevision, &color, &z) != 0;
    if (s_profileLog) { ++s_profile.peeks; s_profile.peek += nowSeconds()-start; }
    static const bool log = getenv("SMS_GX_PEEK_LOG") != nullptr;
    static unsigned logged = 0;
    if (log && logged++ < 64)
        fprintf(stderr, "[gx-aurora] peek frame %llu revision %llu at %d,%d: ARGB %08x Z %06x valid %d, %.3f ms\n",
                (unsigned long long)curFrameId(), (unsigned long long)s_peekRevision, x, y,
                unsigned(color), unsigned(z), int(valid), (nowSeconds() - start) * 1000);
    return valid;
}
uint32_t peekColor(int x, int y) {
    static const bool enablePeek = getenv("SMS_GX_PEEK") != nullptr;
    if (!enablePeek) return 0xff000000u;
    uint32_t color = 0xff000000u, z = 0xffffffu;
    peekPixel(x, y, color, z);
    return color;
}
uint32_t peekZ(int x, int y) {
    static const bool enablePeek = getenv("SMS_GX_PEEK") != nullptr;
    if (!enablePeek) return 0xffffffu;
    uint32_t color = 0xff000000u, z = 0xffffffu;
    peekPixel(x, y, color, z);
    return z;
}

#define METRIC_DUMP_TAG "aurora"
// SMS_GX_METRIC_DUMP=0xADDR:size writes those bytes at reads 12 and 40 of a token (debugging).
static void metricDump(int token, int read) {
    static const char* spec = getenv("SMS_GX_METRIC_DUMP");
    if (!spec || !((read >= 4 && read <= 12) || read == 40)) return;
    unsigned long long addr = 0;
    unsigned size = 0;
    if (sscanf(spec, "%llx:%u", &addr, &size) != 2) return;
    char path[256];
    snprintf(path, sizeof path, "metric_dump_%s_t%d_r%d.bin", METRIC_DUMP_TAG, token, read);
    if (FILE* f = fopen(path, "wb")) {
        fwrite(reinterpret_cast<const void*>(uintptr_t(addr)), 1, size, f);
        fclose(f);
    }
}

void pixMetricClear() {
    if (!s_ready || !s_inFrame) return;
    if (s_metricSlots.size() >= 255) {  // out of counter slots this frame
        metricStop();
        return;
    }
    s_metricSlots.push_back(MetricSlot{});
    s_metricActive = true;
    putAurora(0x0100);  // GX_LOAD_AURORA_PIXEL_METRIC: count into slot size() - 1
    if (fgEnabled()) fgNoteMetric(s_cmd.size());
    put8(uint8_t(s_metricSlots.size()));
}
bool pixMetricDeferred() { return s_metricActive; }
void pixMetricDeferForToken(uint16_t token) {
    static const bool regLog = getenv("SMS_GX_METRIC_REGS") != nullptr;
    static int regLogged = 0;
    if (regLog && regLogged++ < 3) {
        fprintf(stderr, "[gx-aurora] metric token %u: genmode %06x tref %06x %06x tevreg", unsigned(token),
                g.bp[BP_GENMODE], g.bp[BP_TREF], g.bp[BP_TREF + 1]);
        for (int i = 0; i < 8; i++) fprintf(stderr, " %06x", g.bp[BP_TEV_REG + i]);
        fprintf(stderr, " env");
        for (int i = 0; i < 4; i++) fprintf(stderr, " %06x", g.bp[BP_TEV_COLOR_ENV + i]);
        fprintf(stderr, " ksel %06x alphacmp %06x\n", g.bp[BP_TEV_KSEL], g.bp[BP_ALPHACOMPARE]);
    }
    if (s_metricActive && !s_metricSlots.empty()) s_metricSlots.back().token = token;
    metricStop();
    s_metricReadToken = token;
}
void pixMetricDeferDone() { s_metricReadToken = -1; }
uint32_t pixMetricRead() {
    if (s_metricReadToken < 0) return 0;
    auto it = s_tokenValues.find(uint16_t(s_metricReadToken));
    const uint32_t tris = s_metricSlots.empty() ? 0 : s_metricSlots.back().tris;
    const uint32_t v = it != s_tokenValues.end() ? it->second : 0xFFFFFFFFu + tris * 4;
    static const bool metricLog = getenv("SMS_GX_METRIC_LOG") != nullptr;
    static std::unordered_map<int, int> reads;
    static std::unordered_map<int, int> dumpReads;
    metricDump(s_metricReadToken, ++dumpReads[s_metricReadToken]);
    if (metricLog && reads[s_metricReadToken]++ < 12)
        fprintf(stderr, "[gx-aurora] pixel metric token %d read %d = %u\n", s_metricReadToken, reads[s_metricReadToken], v);
    return v;
}

void textureInvalidateAll() { s_texGen++; }

unsigned efbCopyLookup(const void*, int*, int*) { return 0; }

}  // namespace gx

using namespace gx;

// ------------------------------------------------------------------ C entry points
extern "C" {

int GXPC_InitAuto(int) {
    if (s_ready) return 1;
    s_graphicsThread = std::this_thread::get_id();
    if (const char* e = getenv("SMS_GX_STATS")) {
        s_statsEvery = atoi(e) > 0 ? atoi(e) : 0;
    }
    // Aurora's report uses its own coarse counters, not g_decodeSeconds.
    // Do not time every FIFO primitive merely to print the frame statistics.
    g_gxStats = envTrue("SMS_GX_VERTEX_TIMERS");
    if (const char* e = getenv("SMS_GX_FRAMELOG"))
        if (*e && (s_frameLog = fopen(e, "w"))) setvbuf(s_frameLog, nullptr, _IOFBF, 1 << 16);
    if (const char* e = getenv("SMS_GX_PROFILE"))
        if (*e && (s_profileLog = fopen(e, "w"))) setvbuf(s_profileLog, nullptr, _IOFBF, 8192);
    s_profileFine = s_profileLog && !envTrue("SMS_GX_PROFILE_LIGHT");
    SabConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.title = "Super Mario Sunshine";
    cfg.windowWidth = 1280;
    cfg.windowHeight = 960;
    // Widescreen: the window starts at the wide aspect, as the OpenGL port's does.
    if (s_widescreen > 1.0001f) cfg.windowWidth = uint32_t(std::lround(960.0 * 4.0 / 3.0 * s_widescreen));
    if (const char* e = getenv("SMS_WINDOW_SIZE")) sscanf(e, "%ux%u", &cfg.windowWidth, &cfg.windowHeight);
    cfg.backend = backendFromEnv();
    const char* vs = getenv("SMS_VSYNC");
    cfg.vsync = !vs || strcmp(vs, "0") != 0;
    if(vs&&(!strcmp(vs,"off")||!strcmp(vs,"false")))cfg.vsync=0;
    const char* wm = getenv("SMS_WINDOW_MODE");
    cfg.fullscreen = wm && (!strcmp(wm, "fullscreen") || !strcmp(wm, "borderless"));
    cfg.logLevel = envTrue("SMS_AURORA_DEBUG") ? 0 : 1;
    s_mouseCamera = envTrue("SMS_MOUSE_CAMERA");
    if (!sab_init(&cfg)) {
        // sab_init fails only when the window is closed while shaders are prepared:
        // the player quit, and a game with no window must not go on running.
        fprintf(stderr, "[gx-aurora] window closed before the game started\n");
        exit(0);
    }
    fprintf(stderr, "[gx-aurora] Aurora ready (%s)\n", sab_backend_name());
    s_ready = true;
    s_cmd.reserve(2 * 1024 * 1024);
    s_prim.reserve(128 * 1024);
    s_efbW = 640;
    s_efbH = 480;
    sab_set_efb_size(s_efbW, s_efbH);
    const char* tp=getenv("SMS_TEXTURE_PACKS");
    s_texturePacks=!tp||(strcmp(tp,"0")&&strcmp(tp,"off"));
    refreshTexturePacks();
    beginFrame(false);
#ifdef SMS_PARTYBOARD_UI
    GXPC_MenuInit(nullptr, nullptr);
#endif
    // The game quits through exit() (Escape, the window's close button): stop the
    // menu and Aurora's threads first, or the DLL unloads with them running (terminate).
    static bool atExitSet = false;
    if (!atExitSet) {
        atExitSet = true;
        atexit([] { GXPC_Shutdown(); });
    }
    return 1;
}

void GXPC_Shutdown(void) {
    if (!s_ready) return;
#ifdef SMS_PARTYBOARD_UI
    GXPC_MenuShutdown();
#endif
    s_ready = false;
    sab_shutdown();
}

int GXPC_ParseArgs(int* argc, char** argv) {
    int out = 1;
    for (int i = 1; i < *argc; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "--headless") || !strcmp(a, "--window") || !strcmp(a, "--vsync")) continue;
        argv[out++] = argv[i];
    }
    int removed = *argc - out;
    *argc = out;
    argv[out] = nullptr;
    return removed;
}
void GXPC_SetHeadless(int) {}
// Scripted presses (SMS_AUTOPRESS) set the key state directly: the SDL2 event queue is not pumped here.
int GXPC_IsHeadless(void) { return getenv("SMS_AUTOPRESS") ? 1 : 0; }
void GXPC_SetAutoPresent(int on) { s_autoPresent = on != 0; s_haveScanout = false; }
void GXPC_SetVIScanoutCallback(GXPCVIScanoutFn fn) { s_viScanout = fn; s_haveScanout = false; }
void GXPC_PollVI(void) {
    if (!s_ready || s_pollingVI || std::this_thread::get_id() != s_graphicsThread) return;
    s_pollingVI = true;
    if (prepareScanout()) present(true);
    else pumpEvents();
    s_pollingVI = false;
}
void GXPC_Present(const void* xfb) {
    if (s_ready && prepareScanout(xfb,true)) present(true);
}
uint32_t GXPC_FrameCount(void) { return s_frame; }
void GXPC_SetWindowIcon(const uint8_t* rgba, int w, int h) {
    if (s_ready) sab_set_window_icon(rgba, w, h);
}
int GXPC_RunLauncher(const char*, const char*, int) { return 1; }
int GXPC_MouseCaptured(void) { return s_mouseCaptured; }
void GXPC_SetIdleClock(double (*idle)(void)) { s_idleClock = idle; }
int GXPC_GetSpeed(void) { return 1; }
void GXPC_BeginDataPass(unsigned short width, unsigned short height) {
    if (!s_ready || !s_inFrame || s_dataPassActive) return;
    submit();
    ensureCopyReadbackCapacity();
    s_dataPassActive = sab_begin_data_pass(width, height) != 0;
    if (s_dataPassActive) fgNoteDataPass(true, width, height);
}
int GXPC_ReadXFB(const void*, uint8_t*, int*, int*) { return 0; }

void GXPC_InvalidateRange(const void* p, uint32_t size) {
    uintptr_t a = reinterpret_cast<uintptr_t>(p), b = a + size;
    // textures start at most 4 MiB before a written byte they contain
    auto it = s_texByAddr.lower_bound(a > (4u << 20) ? a - (4u << 20) : 0);
    for (; it != s_texByAddr.end() && it->first < b; ++it)
        if (it->first + it->second->bytes > a) it->second->dirty = true;
}

void GXPC_SetWidescreen(float widthOver43) { s_widescreen = widthOver43; }
float GXPC_GetWidescreen(void) { return s_widescreen; }
float GXPC_GetMenuWidescreen(void) { return s_widescreen; }
void GXPC_ApplyMenuWindowSettings(const char* vsync, const char* mode, const char* fullscreenMode,
                                int display, int scale, int mouseCamera) {
    sab_apply_window_settings(vsync,mode,fullscreenMode,display,scale);
    s_mouseCamera=mouseCamera!=0;
}
void GXPC_ApplyMenuGraphicsSettings(int scale,int msaa,int fxaa,int aniso,float gamma,
                                    const char* aspect,const char* filter,float wide,int hudEdges) {
    s_widescreen=std::max(1.f,wide);
    s_hudEdges=hudEdges!=0;
    sab_apply_graphics_settings(scale,msaa,fxaa,aniso,gamma,aspect,filter,s_widescreen);
}
void GXPC_ApplyMenuTexturePacks(int enabled) {
    if(s_texturePacks==(enabled!=0))return;
    s_texturePacks=enabled!=0;refreshTexturePacks();
}
void GXPC_RefreshMenuTexturePacks(void) { refreshTexturePacks(); }
int GXPC_OverlayVisible(void) { return s_overlay; }
void GXPC_OverlayToggle(void) { s_overlay=!s_overlay;sab_set_overlay(s_overlay); }
void GXPC_SetStretch2D(int on) { s_stretch2D=on!=0; }
void GXPC_SetHud(int on) { s_hud=on!=0&&s_hudEdges;s_hudPanes.clear(); }
void GXPC_HudPaneBegin(float x1, float x2) { if(s_hud)s_hudPanes.emplace_back(std::min(x1,x2),std::max(x1,x2)); }
void GXPC_HudPaneEnd(void) { if(s_hud&&!s_hudPanes.empty())s_hudPanes.pop_back(); }
void GXPC_SetNameTags(const GXPCNameTag*, int) {}

void sms_gx_set_event_callback(void (*cb)(const union SDL_Event* ev)) { s_eventCb = cb; }
void sms_gx_pump_events(void) { pumpEvents(); }

}  // extern "C"
