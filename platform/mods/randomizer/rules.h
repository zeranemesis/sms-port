#pragma once

#include <cstdint>

namespace sms_randomizer {

enum class RuleKind { Exit, Shine };
struct Rule {
    std::uint16_t scene;
    RuleKind kind;
    std::uint16_t target;
    std::uint8_t flags;
    std::uint32_t requirements;
};

// TheAzack9's v0.3.6 level constraints, recovered from module function 0x1fd4.
// Entries retain their original order, flags and ability masks.
inline constexpr Rule level_rules[] = {
#include "level_rules.inc"
};

} // namespace sms_randomizer
