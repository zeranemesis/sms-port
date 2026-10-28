#pragma once

#include "rules.h"

namespace sms_randomizer {

// Original 0x450c: a level needs a reachable exit or Shine; the first match wins.
inline bool level_accessible(std::uint16_t scene, std::uint32_t capabilities,
                             unsigned reachable_shines, bool shines_only, bool exits_only)
{
    for (const auto& rule : level_rules) {
        if (rule.scene != scene || (rule.requirements & ~capabilities))
            continue;
        if (rule.kind == RuleKind::Exit && (!shines_only || exits_only) && rule.flags <= reachable_shines)
            return true;
        if (rule.kind == RuleKind::Shine && !exits_only && (rule.target & 0xff) <= reachable_shines)
            return true;
    }
    return false;
}

} // namespace sms_randomizer
