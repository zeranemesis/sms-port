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

#include <dolphin/gx.h>

#include <algorithm>
#include <map>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unordered_map>
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
float s_widescreen = 4.0f / 3.0f;
int s_speed = 1;
bool s_mouseCamera = false, s_mouseCaptured = false, s_mouseReleased = false;
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

void submit() {
    if (s_cmd.empty()) return;
    if (s_inFrame) sab_process(s_cmd.data(), uint32_t(s_cmd.size()));
    s_cmd.clear();
}

// ------------------------------------------------------------------ statistics
struct Stats {
    uint32_t draws = 0, vertices = 0, texLoads = 0, texRehash = 0, copies = 0;
    double submitSeconds = 0, presentSeconds = 0;
} s_stats;
int s_statsEvery = 0;
std::vector<double> s_frameTimes;
double s_lastFrameEnd = 0, s_presentAccum = 0;

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
    s_stats.draws++;
    s_stats.vertices += count;
    if (s_cmd.size() > (4u << 20)) submit();
}

// ------------------------------------------------------------------ frames
void beginFrame() {
    s_inFrame = sab_begin_frame() != 0;
    // Aurora starts each frame with fresh state: send the vertex format again
    // and every texture before its next use.
    s_curFmt = ~0u;
    for (auto& m : s_maps) m.regsDirty = true;
}

void reportStats() {
    if (!s_statsEvery || s_frameTimes.size() < size_t(s_statsEvery)) return;
    std::vector<double> t = s_frameTimes;
    std::sort(t.begin(), t.end());
    double med = t[t.size() / 2] * 1000, p90 = t[t.size() * 9 / 10] * 1000;
    double n = double(s_frameTimes.size());
    fprintf(stderr,
            "[gx-aurora] %s: frame median %.1f ms, p90 %.1f ms; per frame: present %.2f ms, submit %.2f ms, "
            "%.0f draws, %.0f vertices, %.1f texture loads, %.1f rehashes, %.1f copies\n",
            sab_backend_name(), med, p90, s_presentAccum / n * 1000, s_stats.submitSeconds / n * 1000,
            s_stats.draws / n, s_stats.vertices / n, s_stats.texLoads / n, s_stats.texRehash / n, s_stats.copies / n);
    s_frameTimes.clear();
    s_presentAccum = 0;
    s_stats = Stats();
}

void pumpEvents();

void present() {
    double t0 = nowSeconds();
    submit();
    if (s_inFrame) sab_end_frame();
    double t1 = nowSeconds();
    s_presentAccum += t1 - t0;
    if (s_lastFrameEnd > 0) s_frameTimes.push_back(t1 - s_lastFrameEnd);
    s_lastFrameEnd = t1;
    reportStats();
    pumpEvents();
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
            exit(0);
        case SAB_EV_FOCUS_LOST:
            if (s_mouseCamera) captureMouse(false);
            continue;
        case SAB_EV_FOCUS_GAINED:
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
    submit();
    uint32_t src = g.bp[BP_COPY_SRC_TL], size = g.bp[BP_COPY_SRC_WH];
    uint16_t x = uint16_t(src & 0x3FF), y = uint16_t((src >> 10) & 0x3FF);
    uint16_t w = uint16_t((size & 0x3FF) + 1), h = uint16_t(((size >> 10) & 0x3FF) + 1);
    const bool disp = (ctrl >> 14) & 1;
    const bool clear = (ctrl >> 11) & 1;
    void* dest = g.copyDest;
    if (disp) {
        if (w != s_efbW || h != s_efbH) {
            s_efbW = w;
            s_efbH = h;
            sab_set_efb_size(w, h);
        }
        s_frame++;
        if (g_displayCopyHook) g_displayCopyHook(dest);
        present();
        return;
    }
    const bool half = (ctrl >> 9) & 1;
    uint16_t dw = half ? uint16_t(w / 2) : w, dh = half ? uint16_t(h / 2) : h;
    if (s_inFrame) sab_copy_tex(dest, x, y, w, h, dw ? dw : 1, dh ? dh : 1, g_texCopyFmt, clear ? 1 : 0);
    s_stats.copies++;
}

uint32_t peekColor(int, int) { return 0xFF000000u; }
uint32_t peekZ(int, int) { return 0xFFFFFF; }

// Pixel metrics (Delfino's pollution count) are not implemented on Aurora yet.
void pixMetricClear() {}
uint32_t pixMetricRead() { return 0; }
bool pixMetricDeferred() { return false; }
void pixMetricDeferForToken(uint16_t) {}
void pixMetricDeferDone() {}

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
    if (const char* e = getenv("SMS_WINDOW_SIZE")) sscanf(e, "%ux%u", &cfg.windowWidth, &cfg.windowHeight);
    cfg.backend = backendFromEnv();
    const char* vs = getenv("SMS_VSYNC");
    cfg.vsync = !vs || strcmp(vs, "0") != 0;
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
    beginFrame();
    return 1;
}

void GXPC_Shutdown(void) {
    if (!s_ready) return;
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
int GXPC_IsHeadless(void) { return 0; }
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
void GXPC_SetStretch2D(int) {}
void GXPC_SetHud(int) {}
void GXPC_HudPaneBegin(float, float) {}
void GXPC_HudPaneEnd(void) {}
void GXPC_SetNameTags(const GXPCNameTag*, int) {}

void sms_gx_set_event_callback(void (*cb)(const union SDL_Event* ev)) { s_eventCb = cb; }
void sms_gx_pump_events(void) { pumpEvents(); }

}  // extern "C"
