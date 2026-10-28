#pragma once

#include <array>
#include <cstdint>

namespace sms_randomizer {

// Original module offset 0x1d04: restore unlocks from the 120 collected Shines.
inline std::uint32_t collected_unlocks(const std::array<std::uint32_t, 120>& rewards,
                                     const std::array<bool, 120>& collected,
                                     std::uint32_t initial)
{
    for (unsigned i = 0; i < rewards.size(); ++i)
        if (collected[i])
            initial |= rewards[i];
    return initial;
}

} // namespace sms_randomizer
