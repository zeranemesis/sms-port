#pragma once

#include <cstdint>

namespace sms_randomizer {

// Translated from Randomizer v0.3.6 KXER code offsets 0x6b64..0x6bf8.
struct Nozzles {
    int backup;
    int primary;
    bool remove_fludd;
};

constexpr Nozzles select_nozzles(bool spray, bool hover, bool rocket, bool turbo)
{
    const int backup = hover ? 4 : rocket ? 1 : turbo ? 5 : -1;
    return {backup, spray ? 0 : -1, !spray && backup == -1};
}

// Original changeNozzle gate (0x5b60), for the six valid nozzle types.
constexpr bool can_use_nozzle(std::uint32_t unlocks, unsigned nozzle)
{
    constexpr unsigned bits[] = {17, 20, 19, 14, 19, 21};
    return nozzle < 6 && (unlocks & (1u << bits[nozzle])) != 0;
}

// Original backup-button gate (0x5af4); no FLUDD is present if neither is owned.
constexpr bool can_switch_backup(std::uint32_t unlocks)
{
    return ((unlocks & (1u << 17)) != 0) ==
           ((unlocks & ((1u << 19) | (1u << 20) | (1u << 21))) != 0);
}

} // namespace sms_randomizer
