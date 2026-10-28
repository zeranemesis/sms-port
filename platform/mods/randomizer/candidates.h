#pragma once

#include "access.h"

namespace sms_randomizer {

// Original 0x3f48: missing unlocks that can satisfy the requested access bits.
inline std::vector<std::uint32_t> candidate_unlocks(std::uint32_t required,
                                                  std::uint32_t unlocks,
                                                  const std::vector<std::uint8_t>& shines)
{
    std::vector<std::uint32_t> result;
    const auto add = [&](std::uint32_t value) {
        if (!(unlocks & value))
            result.push_back(value);
    };
    if (required & 1) {
        add(1);
        add(0x80000);
        add(4);
    }
    if ((required & 2) && (access_mask(unlocks, shines) & 1)) {
        add(0x20);
        add(2);
        if (unlocks & 0x80000) {
            add(1);
            add(4);
        }
        if (unlocks & 5)
            add(0x80000);
    }
    constexpr std::pair<unsigned, unsigned> bits[] = {
        {2, 17}, {3, 19}, {6, 20}, {4, 8}, {5, 7}, {7, 21}, {8, 10},
        {9, 6}, {10, 3}, {11, 15}, {12, 22}, {13, 14}, {14, 9}, {15, 16},
    };
    for (const auto& bit : bits)
        if (required & (1u << bit.first))
            add(1u << bit.second);
    return result;
}

} // namespace sms_randomizer
