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
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace gx {

bool g_gxStats = false;
double g_flushSeconds = 0;
void (*g_displayCopyHook)(const void* xfb) = nullptr;
extern uint32_t g_texCopyFmt;  // gx_api.cpp: the GXTexFmt of the last GXSetTexCopyDst

namespace {

bool s_ready = false;
bool s_inFrame = false;
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
int s_speed = 1;
bool s_mouseCamera = false, s_mouseCaptured = false, s_mouseReleased = false;
bool s_hasFocus = true;
bool s_overlay = false;
GXPCVIScanoutFn s_viScanout = nullptr;
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

double s_submitSeconds = 0;  // statistics: Aurora's command processing on this thread

void submit() {
    if (s_cmd.empty()) return;
    if (s_inFrame) {
        timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        sab_process(s_cmd.data(), uint32_t(s_cmd.size()));
        clock_gettime(CLOCK_MONOTONIC, &t1);
        s_submitSeconds += double(t1.tv_sec - t0.tv_sec) + double(t1.tv_nsec - t0.tv_nsec) * 1e-9;
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

void metricStop() {
    if (!s_metricActive) return;
    s_metricActive = false;
    putAurora(0x0100);  // GX_LOAD_AURORA_PIXEL_METRIC
    put8(0);
}

void endFrameMetrics() {
    metricStop();
    if (!s_metricSlots.empty() && s_frameId)
        s_metricFrames[s_frameId] = MetricFrame{std::move(s_metricSlots), sab_efb_pixel_area_scale()};
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
                    const double perPixel = double(area > 0 ? area : 1.0f) * double(slots[i].drawScale);
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
    uint64_t frameId;
    uint64_t hash;  // the destination's bytes when the copy was made (or last written back)
    uint32_t bytes, fmt, layout;
    bool intensity, alphaOne;
};
std::map<const void*, std::vector<PendingCopy>> s_pendingCopies;
std::unordered_set<const void*> s_efbCopyDestinations;
uint32_t s_writeBackCount = 0;  // statistics

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

bool writeFrameDump(const void* marker, uint32_t w, uint32_t h, const uint32_t* rgba) {
    auto it = s_dumpPending.find(marker);
    if (it == s_dumpPending.end()) return false;
    char path[1024];
    snprintf(path, sizeof path, "%s/frame_%06u.ppm", it->second.dir.c_str(), it->second.frame);
    if (FILE* f = fopen(path, "wb")) {
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

void storeCopyReadbacks() {
    const void* dest;
    uint32_t w, h;
    uint64_t frameId;
    const uint32_t* rgba;
    static std::vector<uint8_t> px, enc;
    while (sab_poll_copy_readback(&dest, &w, &h, &frameId, &rgba)) {
        if (writeFrameDump(dest, w, h, rgba)) continue;
        auto it = s_pendingCopies.find(dest);
        if (it == s_pendingCopies.end()) continue;
        auto& q = it->second;
        while (!q.empty() && q.front().frameId < frameId) q.erase(q.begin());  // read-backs that never came
        if (q.empty() || q.front().frameId != frameId) continue;
        PendingCopy pc = q.front();
        q.erase(q.begin());
        if (hashBytes(dest, pc.bytes) == pc.hash) {  // otherwise the CPU wrote them after the copy: it wins
            px.resize(size_t(w) * h * 4);
            memcpy(px.data(), rgba, px.size());
            for (size_t i = 0; i < px.size(); i += 4) convertCopyPixel(&px[i], pc.fmt, pc.intensity, pc.alphaOne);
            enc.resize(texLevelBytes(pc.layout, w, h));
            uint32_t n = encodeTexture(px.data(), pc.layout, w, h, enc.data());
            if (n > pc.bytes) n = pc.bytes;
            memcpy(const_cast<void*>(dest), enc.data(), n);
            const uint64_t stored = hashBytes(dest, pc.bytes);
            for (auto& later : q)  // our own write is not a CPU change for the copies still pending
                if (later.hash == pc.hash) later.hash = stored;
            GXPC_InvalidateRange(dest, pc.bytes);
            s_writeBackCount++;
        }
        if (q.empty()) s_pendingCopies.erase(it);
    }
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
double s_acquireAccum = 0;
int s_acquireLong = 0;

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
    uint64_t hash;
    uint32_t bytes;
    uint32_t checkedGen;
    bool dirty;
};
std::unordered_map<TexKey, TexEntry, TexKeyHash> s_textures;
std::multimap<uintptr_t, TexEntry*> s_texByAddr;
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
};
MapState s_maps[8];

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
    uint64_t h = hashBytes(src, n * 2, uint64_t(*tlutFmt) << 32 | n);
    auto& slot = s_tluts[h];
    if (!slot) {
        slot.reset(new TlutEntry);
        slot->id = s_nextTlutId++;
        slot->data.assign(src, src + n * 2);
    }
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
    uint32_t mask = usedMaps();
    for (int m = 0; m < 8; m++) {
        if (!(mask & (1u << m))) continue;
        const uint8_t* ptr = g.texImage[m];
        if (!ptr) continue;
        uint32_t image0 = g.bp[bpTexReg(BP_TX_IMAGE0, m)];
        uint32_t levels = texLevels(m);
        TexEntry& e = textureEntry(ptr, image0, levels);
        recheck(e, ptr);
        uint32_t fmt = (image0 >> 20) & 0xF;
        uint32_t tlutId = 0;
        const uint8_t* tlutData = nullptr;
        uint16_t tlutEntries = 0;
        uint32_t tlutFmt = 0;
        bool ci = fmt == GX_TF_C4 || fmt == GX_TF_C8 || fmt == GX_TF_C14X2;
        if (ci) tlutId = tlutFor(m, fmt, &tlutData, &tlutEntries, &tlutFmt);
        MapState& ms = s_maps[m];
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
inline void copyBE32(uint8_t* out, const uint8_t* in, uint32_t words) {
    for (uint32_t i = 0; i < words; i++) {
        uint32_t v;
        memcpy(&v, in + i * 4, 4);
        v = __builtin_bswap32(v);
        memcpy(out + i * 4, &v, 4);
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
    const float sx=xfFloat(XFR_VIEWPORT),cx=xfFloat(XFR_VIEWPORT+3)-342.f;
    const bool full=std::fabs(std::fabs(sx)*2.f-640.f)<2.f&&std::fabs(cx-320.f)<2.f;
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
    if(!full) return {1.f/wide,anchorOffset(cx)/wide};
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

void emitDraw(uint8_t opcode, uint32_t count) {
    const VtxFmtLayout& L = vtxFmtLayout(s_primFmt);
    const uint32_t fmt = s_primFmt;
    const bool mtx = fmt & VF_MTX;
    const bool nrm = fmt & VF_NRM;
    const uint32_t nrmBytes = (fmt & VF_NBT) ? 36 : 12;
    uint32_t outStride = (mtx ? 9 : 0) + 12 + (nrm ? nrmBytes : 0);
    for (int c = 0; c < 2; c++)
        if (fmt & (VF_CLR0 << c)) outStride += 4;
    for (int t = 0; t < 8; t++)
        if (fmt & (VF_TEX0 << t)) outStride += 8;
    setVertexFormat(fmt);
    syncTextures();
    syncTexCoordScales();
    const auto mapping=drawMapping(count);
    const bool mapped=mapping.scale!=1.f||mapping.offset!=0.f;
    const uint32_t tl=g.bp[BP_SCISSOR_TL],br=g.bp[BP_SCISSOR_BR];
    if (s_metricActive && !s_metricSlots.empty()) s_metricSlots.back().drawScale = mapping.scale;
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
    // A draw holds at most 0xFFFF vertices; the loader never makes bigger ones.
    put8(uint8_t(opcode | 0));  // vertex attribute format 0
    put16(uint16_t(count));
    size_t base = s_cmd.size();
    s_cmd.resize(base + size_t(count) * outStride);
    uint8_t* out = &s_cmd[base];
    const uint8_t* in = s_prim.data();
    for (uint32_t v = 0; v < count; v++, in += s_primStride) {
        if (mtx) {
            memcpy(out, in + L.mtx, 9);
            out += 9;
        }
        copyBE32(out, in, 3);
        out += 12;
        if (nrm) {
            copyBE32(out, in + L.nrm, nrmBytes / 4);
            out += nrmBytes;
        }
        for (int c = 0; c < 2; c++)
            if (fmt & (VF_CLR0 << c)) {
                memcpy(out, in + L.clr[c], 4);
                out += 4;
            }
        for (int t = 0; t < 8; t++)
            if (fmt & (VF_TEX0 << t)) {
                copyBE32(out, in + L.tex[t], 2);
                out += 8;
            }
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

// ------------------------------------------------------------------ frames
void beginFrame() {
    const double t0 = nowSeconds();
    s_inFrame = sab_begin_frame() != 0;
    const double waited = nowSeconds() - t0;
    s_acquireAccum += waited;
    s_acquireLong += waited > 0.004;
    s_frameId = s_inFrame ? sab_frame_id() : 0;
    // Aurora starts each frame with fresh state: send the vertex format again
    // and every texture before its next use.
    s_curFmt = ~0u;
    for (auto& m : s_maps) m.regsDirty = true;
    for (bool& v : s_suValid) v = false;
}

void reportStats() {
    if (!s_statsEvery || s_frameTimes.size() < size_t(s_statsEvery)) return;
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

void present() {
    double t0 = nowSeconds();
    endFrameMetrics();
    submit();
    if (s_inFrame) sab_end_frame();
    s_inFrame = false;
    collectPixelMetrics();
    storeCopyReadbacks();
    double t1 = nowSeconds();
    s_presentAccum += t1 - t0;
    if (s_lastFrameEnd > 0) s_frameTimes.push_back(t1 - s_lastFrameEnd);
    s_lastFrameEnd = t1;
    reportStats();
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
            if (s_mouseCamera && !s_mouseReleased) captureMouse(true);
            continue;
        case SAB_EV_MOUSEBUTTONDOWN:
            if (s_mouseCamera && !s_mouseCaptured) {
                s_mouseReleased = false;
                captureMouse(true);
            }
            continue;
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
                if (fresh) s_speed = s_speed >= 4 ? 1 : s_speed * 2;
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
        s_maps[(reg & 3) + ((reg & 0x20) ? 4 : 0)].regsDirty = true;  // Aurora forgets the texture's size
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
    if ((reg & 0xF0) == CP_MATIDX_A || (reg & 0xF0) == CP_MATIDX_B) putCP(reg, value);
}

void sinkXF(uint16_t addr, uint32_t count, const uint32_t* words) {
    put8(0x10);
    put32((count - 1) << 16 | addr);
    size_t n = s_cmd.size();
    s_cmd.resize(n + size_t(count) * 4);
    memcpy(&s_cmd[n], words, size_t(count) * 4);
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
void synchronizeReadbacks() {}

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
    if(dest&&!(ctrl&(1u<<14)))s_efbCopyDestinations.insert(dest);
    if (disp) {
        if (w != s_efbW || h != s_efbH) {
            s_efbW = w;
            s_efbH = h;
            sab_set_efb_size(w, h);
        }
        s_frame++;
        static const uint32_t dumpFrom = getenv("SMS_GX_DUMP_FROM") ? uint32_t(atoi(getenv("SMS_GX_DUMP_FROM"))) : 0;
        const bool dumpThis = dumpDir() && s_frame >= dumpFrom && s_frame % uint32_t(dumpEvery()) == 0;
        const bool captureThis = s_frame <= s_captureUntil && s_frame % 2 == 0;
        if (s_inFrame && (dumpThis || captureThis)) {
            void* marker = &s_dumpMarkers[s_dumpNext++ % sizeof s_dumpMarkers];
            if (!s_dumpPending.count(marker)) {
                s_dumpPending[marker] = PendingDump{s_frame, captureThis ? s_captureDir : std::string(dumpDir())};
                sab_copy_tex(marker, x, y, w, h, w, h, GX_TF_RGBA8, 0, 1);
            }
        }
        if (g_displayCopyHook) g_displayCopyHook(dest);
        present();
        return;
    }
    const bool half = (ctrl >> 9) & 1;
    uint16_t dw = half ? uint16_t(w / 2) : w, dh = half ? uint16_t(h / 2) : h;
    if (!dw) dw = 1;
    if (!dh) dh = 1;
    // Widescreen: draws that do not span the screen land in the centred 4:3 area
    // (drawMapping), so a copy of a part of the screen is taken from there too,
    // as the OpenGL port does. A full-width copy, or one under Stretch2D, takes
    // the whole width.
    uint16_t cx = x, cw = w;
    if (s_widescreen > 1.0001f && !s_stretch2D && !(x == 0 && w >= 640)) {
        const float wide = s_widescreen, margin = 320.f * (wide - 1.f);
        const float l = (float(x) + margin) / wide, r = (float(x + w) + margin) / wide;
        cx = uint16_t(std::lround(l));
        cw = uint16_t(std::max(1l, std::lround(r) - long(cx)));
    }
    const uint32_t f = ((ctrl >> 3) & 1) << 3 | ((ctrl >> 4) & 7);
    const bool zcopy = (g.bp[BP_PE_CONTROL] & 7) == 3;
    // Only data copies come back to RAM (Delfino's pollution layers are R8 into I8):
    // the colour copies are screen effects the CPU does not read.
    // SMS_GX_COPY_WRITEBACK=all writes every copy back.
    const uint32_t layout = copyLayout(f, false);
    const bool readback = s_inFrame && dest && !zcopy && copyWriteBackEnabled() &&
                          (layout <= 3 || copyWriteBackAll());
    if (readback) {
        PendingCopy pc{};
        pc.frameId = s_frameId;
        pc.fmt = f;
        pc.layout = copyLayout(f, false);
        pc.bytes = texLevelBytes(pc.layout, dw, dh);
        pc.intensity = ((ctrl >> 15) & 3) == 3;
        pc.alphaOne = (g.bp[BP_PE_CONTROL] & 7) != 1;  // only RGBA6_Z24 keeps alpha
        pc.hash = hashBytes(dest, pc.bytes);
        s_pendingCopies[dest].push_back(pc);
    }
    static const bool copyLog = getenv("SMS_GX_COPY_LOG") != nullptr;
    if (copyLog) {
        static std::map<std::tuple<const void*, uint32_t, uint32_t, uint32_t>, int> seen;
        if (seen[{dest, uint32_t(dw) << 16 | dh, f, (((ctrl >> 15) & 3) == 3)}]++ == 0)
            fprintf(stderr, "[gx-aurora] copy %p %ux%u (src %ux%u at %u,%u) fmt %u intensity %d layout %u, %u bytes\n",
                    dest, dw, dh, w, h, x, y, f, int((((ctrl >> 15) & 3) == 3)), copyLayout(f, false), texLevelBytes(copyLayout(f, false), dw, dh));
    }
    if (s_inFrame) sab_copy_tex(dest, cx, y, cw, h, dw, dh, g_texCopyFmt, clear ? 1 : 0, readback ? 1 : 0);
    s_stats.copies++;
}

uint32_t peekColor(int, int) { return 0xFF000000u; }
uint32_t peekZ(int, int) { return 0xFFFFFF; }

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
    if (const char* e = getenv("SMS_GX_STATS")) {
        s_statsEvery = atoi(e) > 0 ? atoi(e) : 0;
        g_gxStats = s_statsEvery > 0;
    }
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
        fprintf(stderr, "[gx-aurora] Aurora failed to start\n");
        return 0;
    }
    fprintf(stderr, "[gx-aurora] Aurora ready (%s)\n", sab_backend_name());
    s_ready = true;
    s_efbW = 640;
    s_efbH = 480;
    sab_set_efb_size(s_efbW, s_efbH);
    const char* tp=getenv("SMS_TEXTURE_PACKS");
    s_texturePacks=!tp||(strcmp(tp,"0")&&strcmp(tp,"off"));
    refreshTexturePacks();
    beginFrame();
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
void GXPC_SetAutoPresent(int) {}
void GXPC_SetVIScanoutCallback(GXPCVIScanoutFn fn) { s_viScanout = fn; }
void GXPC_PollVI(void) { pumpEvents(); }
void GXPC_Present(const void*) {}
uint32_t GXPC_FrameCount(void) { return s_frame; }
void GXPC_SetWindowIcon(const uint8_t* rgba, int w, int h) {
    if (s_ready) sab_set_window_icon(rgba, w, h);
}
int GXPC_RunLauncher(const char*, const char*, int) { return 1; }
int GXPC_MouseCaptured(void) { return s_mouseCaptured; }
void GXPC_SetIdleClock(double (*idle)(void)) { s_idleClock = idle; }
int GXPC_GetSpeed(void) { return s_speed; }
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
