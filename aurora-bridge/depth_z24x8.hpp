#pragma once

#include <cstdint>
#include <cstddef>
#include <utility>
#include <vector>

namespace sms_aurora_depth {
using std::size_t;
// GPU depth values have already been converted from reversed Z to forward GX
// uint24. Keep the texture's two planes big-endian; CPU readers must decode BE.
inline bool encode_z24x8(const uint32_t* depth, uint32_t srcWidth, uint32_t srcHeight,
                         uint32_t dstWidth, uint32_t dstHeight, std::vector<uint8_t>& out) {
    if (!depth || !srcWidth || !srcHeight || !dstWidth || !dstHeight ||
        srcWidth > 1024 || srcHeight > 1024 || dstWidth > 1024 || dstHeight > 1024) return false;
    const uint32_t tilesX = (dstWidth + 3) / 4, tilesY = (dstHeight + 3) / 4;
    std::vector<uint8_t> encoded(size_t(tilesX) * tilesY * 64);
    for (uint32_t y = 0; y < tilesY * 4; ++y) {
        const uint32_t dy = y < dstHeight ? y : dstHeight - 1;
        const uint32_t sy = uint32_t((uint64_t(2 * dy + 1) * srcHeight) / (2 * dstHeight));
        for (uint32_t x = 0; x < tilesX * 4; ++x) {
            const uint32_t dx = x < dstWidth ? x : dstWidth - 1;
            const uint32_t sx = uint32_t((uint64_t(2 * dx + 1) * srcWidth) / (2 * dstWidth));
            const uint32_t z = depth[size_t(sy) * srcWidth + sx] & 0x00ffffffu;
            const size_t tile = size_t(y / 4) * tilesX + x / 4;
            const size_t pixel = (y % 4) * 4 + x % 4;
            const size_t ar = tile * 64 + pixel * 2, gb = ar + 32;
            encoded[ar] = 0xff;
            encoded[ar + 1] = uint8_t(z >> 16);
            encoded[gb] = uint8_t(z >> 8);
            encoded[gb + 1] = uint8_t(z);
        }
    }
    out = std::move(encoded);
    return true;
}
} // namespace sms_aurora_depth
