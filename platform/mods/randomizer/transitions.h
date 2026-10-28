#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace sms_randomizer {

// KXER v0.3.6 offsets 0x1ec4 and 0x1f14 use first-match lookup, then identity.
class Transitions {
public:
    std::vector<std::pair<std::uint16_t, std::uint16_t>> exits;
    std::vector<std::pair<std::uint16_t, std::uint16_t>> level_indices;

    std::uint16_t swap_exit(std::uint16_t scene) const
    {
        for (const auto& entry : exits)
            if (entry.first == scene)
                return entry.second;
        return scene;
    }

    std::uint16_t level_index(std::uint16_t scene) const
    {
        for (const auto& entry : level_indices)
            if (entry.first == scene)
                return entry.second;
        return scene;
    }
};

} // namespace sms_randomizer
