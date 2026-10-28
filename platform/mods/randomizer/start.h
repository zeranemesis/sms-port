#pragma once

#include "access.h"
#include "eligibility.h"
#include <algorithm>
#include <iterator>
#include <optional>

extern "C" void sms_msl_srand(unsigned int seed);
extern "C" int sms_msl_rand(void);

namespace sms_randomizer {

// Original seed setup 0x5cc..0x920: keep the plaza routes fixed, then choose
// the first destination from scenes with an immediately accessible Shine.
inline std::vector<std::uint16_t> starting_destinations(std::uint32_t unlocks,
                                                      const std::vector<std::uint16_t>& scenes)
{
    constexpr std::uint16_t fixed[] = {
        0x0100, 0x0101, 0x0102, 0x0105, 0x0106, 0x0107, 0x0108, 0x0109, 0x3400, 0x3c00,
    };
    const auto capabilities = access_mask(unlocks, {});
    std::vector<std::uint16_t> result;
    for (const auto scene : scenes) {
        if (std::find(std::begin(fixed), std::end(fixed), scene) != std::end(fixed))
            continue;
        if (scene == 0x1400 || scene == 0x3400)
            continue;
        if (level_accessible(scene, capabilities, 0, true, false))
            result.push_back(scene);
    }
    return result;
}

inline std::optional<std::uint16_t> starting_destination(std::uint32_t seed, std::uint32_t unlocks,
                                                       const std::vector<std::uint16_t>& scenes)
{
    const auto destinations = starting_destinations(unlocks, scenes);
    if (destinations.empty())
        return std::nullopt;
    sms_msl_srand(seed);
    return destinations[static_cast<unsigned>(sms_msl_rand()) % destinations.size()];
}

} // namespace sms_randomizer
