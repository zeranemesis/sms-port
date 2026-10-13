// GameCube texture layouts shared by the renderers: level sizes, and the
// encoder EFB copies use to store texels in RAM the way GXCopyTex does.
#include "gx_internal.h"

namespace gx {

static const uint8_t kTileW[16] = {8, 8, 8, 4, 4, 4, 4, 0, 8, 8, 4, 0, 0, 0, 8, 0};
static const uint8_t kTileH[16] = {8, 4, 4, 4, 4, 4, 4, 0, 8, 4, 4, 0, 0, 0, 8, 0};
static const uint8_t kTileBytes[16] = {32, 32, 32, 32, 32, 32, 64, 0, 32, 32, 32, 0, 0, 0, 32, 0};

uint32_t texLevelBytes(uint32_t fmt, uint32_t w, uint32_t h) {
    fmt &= 15;
    if (!kTileW[fmt]) return 0;
    uint32_t cols = (w + kTileW[fmt] - 1) / kTileW[fmt];
    uint32_t rows = (h + kTileH[fmt] - 1) / kTileH[fmt];
    return cols * rows * kTileBytes[fmt];
}

// Memory layout (texture format) an EFB copy of copy-format nibble `f`
// produces, as GXCopyTex would store it.
uint32_t copyLayout(uint32_t f, bool z) {
    switch (f) {
    case 0: return 0;                               // I4 / R4 / Z4
    case 2: return 2;                               // IA4 / RA4
    case 3: case 11: case 12: return 3;             // IA8 / RA8 / RG8 / GB8 / Z16*
    case 4: return z ? 3 : 4;                       // RGB565
    case 5: return 5;                               // RGB5A3
    case 6: return 6;                               // RGBA8 / Z24X8
    default: return 1;                              // I8, A8, R8, G8, B8, Z8*
    }
}

// Inverse of decodeTexture for the formats EFB copies produce: `rgba` is
// w*h texels, top row first, in the form decodeTexture returns.
uint32_t encodeTexture(const uint8_t* rgba, uint32_t fmt, uint32_t w, uint32_t h, uint8_t* dst) {
    uint32_t tw = kTileW[fmt], th = kTileH[fmt];
    if (!tw) return 0;
    uint32_t cols = (w + tw - 1) / tw, rows = (h + th - 1) / th;
    uint8_t* p = dst;
    static const uint8_t kZero[4] = {0, 0, 0, 0};
    for (uint32_t ty = 0; ty < rows; ty++) {
        for (uint32_t tx = 0; tx < cols; tx++) {
            auto at = [&](uint32_t i, uint32_t tileW) -> const uint8_t* {
                uint32_t x = tx * tw + i % tileW, y = ty * th + i / tileW;
                return (x < w && y < h) ? rgba + (size_t(y) * w + x) * 4 : kZero;
            };
            switch (fmt) {
            case 0:  // I4
                for (uint32_t i = 0; i < 64; i += 2)
                    p[i >> 1] = uint8_t((at(i, 8)[0] * 15 + 127) / 255 << 4 | (at(i + 1, 8)[0] * 15 + 127) / 255);
                break;
            case 1:  // I8
                for (uint32_t i = 0; i < 32; i++) p[i] = at(i, 8)[0];
                break;
            case 2:  // IA4
                for (uint32_t i = 0; i < 32; i++) {
                    const uint8_t* c = at(i, 8);
                    p[i] = uint8_t((c[3] * 15 + 127) / 255 << 4 | (c[0] * 15 + 127) / 255);
                }
                break;
            case 3:  // IA8
                for (uint32_t i = 0; i < 16; i++) {
                    const uint8_t* c = at(i, 4);
                    p[2 * i] = c[3];
                    p[2 * i + 1] = c[0];
                }
                break;
            case 4:  // RGB565
                for (uint32_t i = 0; i < 16; i++) {
                    const uint8_t* c = at(i, 4);
                    uint16_t v = uint16_t((c[0] >> 3) << 11 | (c[1] >> 2) << 5 | (c[2] >> 3));
                    p[2 * i] = uint8_t(v >> 8);
                    p[2 * i + 1] = uint8_t(v);
                }
                break;
            case 5:  // RGB5A3
                for (uint32_t i = 0; i < 16; i++) {
                    const uint8_t* c = at(i, 4);
                    uint16_t v;
                    if (c[3] >= 0xE0)
                        v = uint16_t(0x8000 | (c[0] >> 3) << 10 | (c[1] >> 3) << 5 | (c[2] >> 3));
                    else
                        v = uint16_t((c[3] >> 5) << 12 | (c[0] >> 4) << 8 | (c[1] >> 4) << 4 | (c[2] >> 4));
                    p[2 * i] = uint8_t(v >> 8);
                    p[2 * i + 1] = uint8_t(v);
                }
                break;
            case 6:  // RGBA8: AR plane then GB plane
                for (uint32_t i = 0; i < 16; i++) {
                    const uint8_t* c = at(i, 4);
                    p[2 * i] = c[3];
                    p[2 * i + 1] = c[0];
                    p[32 + 2 * i] = c[1];
                    p[32 + 2 * i + 1] = c[2];
                }
                break;
            }
            p += kTileBytes[fmt];
        }
    }
    return uint32_t(p - dst);
}

}  // namespace gx
