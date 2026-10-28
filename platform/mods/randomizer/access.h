#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace sms_randomizer {

// Original module 0x3840: unlock bits and access-requirement bits differ.
inline std::uint32_t access_mask(std::uint32_t unlocks,
                                const std::vector<std::uint8_t>& reachable_shines)
{
    std::uint32_t result = (unlocks & 0x80005) ? 1 : 0;
    if ((unlocks & 0x22) || ((unlocks & 0x80000) && (unlocks & 5)))
        result |= 2;
    constexpr std::pair<unsigned, unsigned> bits[] = {
        {17, 2}, {19, 3}, {8, 4}, {7, 5}, {20, 6}, {21, 7}, {10, 8},
        {6, 9}, {3, 10}, {15, 11}, {22, 12}, {9, 14}, {16, 15},
    };
    for (const auto& bit : bits)
        if (unlocks & (1u << bit.first))
            result |= 1u << bit.second;
    if ((unlocks & 8) && (unlocks & 0x4000))
        result |= 0x2000;
    unsigned shadow_marios = 0;
    for (const auto shine : reachable_shines)
        if (shine >= 6 && shine <= 66 && (shine - 6) % 10 == 0)
            ++shadow_marios;
    if (shadow_marios == 7)
        result |= 0x10000;
    return result;
}

} // namespace sms_randomizer
