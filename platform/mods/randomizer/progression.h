#pragma once

#include "entrances.h"
#include "candidates.h"

namespace sms_randomizer {

struct ProgressionReach {
    std::set<std::uint16_t> scenes;
    std::set<std::uint8_t> shines;
    std::uint32_t unlocks = 0x400;
};

struct ProgressiveSeed {
    EntranceSeed entrances;
    std::array<std::uint32_t, 120> rewards{};

    ProgressionReach reachable(const std::vector<std::uint16_t>& available) const
    {
        ProgressionReach reach;
        reach.scenes.insert(entrances.destination(0));
        bool changed = true;
        while (changed) {
            changed = false;
            const std::vector<std::uint8_t> ids(reach.shines.begin(), reach.shines.end());
            const auto capabilities = access_mask(reach.unlocks, ids);
            for (const auto& rule : level_rules) {
                if (!reach.scenes.count(rule.scene) || (rule.requirements & ~capabilities))
                    continue;
                if (rule.kind == RuleKind::Shine && (rule.target & 255) <= reach.shines.size()) {
                    const auto id = static_cast<std::uint8_t>(rule.target >> 8);
                    if (id >= rewards.size())
                        throw std::runtime_error("Invalid Shine reward index");
                    changed |= reach.shines.insert(id).second;
                    const auto unlocked = reach.unlocks | rewards[id];
                    changed |= unlocked != reach.unlocks;
                    reach.unlocks = unlocked;
                    if (std::find(available.begin(), available.end(), 0x0108) != available.end())
                        changed |= reach.scenes.insert(0x0108).second;
                } else if (rule.kind == RuleKind::Exit && rule.flags <= reach.shines.size()) {
                    const auto destination = entrances.destination(rule.target);
                    if (std::find(available.begin(), available.end(), destination) != available.end())
                        changed |= reach.scenes.insert(destination).second;
                }
            }
        }
        return reach;
    }
};

// Preview progression format 2; original access rules and weighted candidates,
// with a native placement algorithm. Console seed compatibility is not implied.
inline ProgressiveSeed make_progressive_seed(std::uint32_t seed,
                                            const std::vector<std::uint16_t>& available)
{
    const auto starts = starting_destinations(0x400, available);
    if (starts.empty())
        throw std::runtime_error("No starting scene accessible without unlocks");
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        ProgressiveSeed result;
        result.entrances = make_entrance_seed(seed + attempt * 0x85ebca6bu, available);
        const auto first = starts[static_cast<unsigned>(sms_msl_rand()) % starts.size()];
        const auto replacement = std::find_if(result.entrances.routes.begin(), result.entrances.routes.end(),
            [first](const auto& route) { return route.second == first; });
        if (replacement == result.entrances.routes.end())
            throw std::runtime_error("Starting scene is absent from the entrance layout");
        std::swap(result.entrances.routes.at(0), replacement->second);
        bool failed = false;
        for (unsigned step = 0; step < 22; ++step) {
            const auto reach = result.reachable(available);
            std::vector<std::uint8_t> slots;
            for (const auto id : reach.shines)
                if (!result.rewards[id])
                    slots.push_back(id);
            if (slots.empty()) { failed = true; break; }
            const std::vector<std::uint8_t> ids(reach.shines.begin(), reach.shines.end());
            const auto capabilities = access_mask(reach.unlocks, ids);
            std::vector<std::uint32_t> candidates;
            for (const auto& rule : level_rules) {
                if (!reach.scenes.count(rule.scene))
                    continue;
                const auto choices = candidate_unlocks(rule.requirements & ~capabilities, reach.unlocks, ids);
                candidates.insert(candidates.end(), choices.begin(), choices.end());
            }
            if (candidates.empty())
                for (unsigned bit = 0; bit < 23; ++bit)
                    if (!(reach.unlocks & (1u << bit)))
                        candidates.push_back(1u << bit);
            if (candidates.empty()) { failed = true; break; }
            const auto reward = candidates[static_cast<unsigned>(sms_msl_rand()) % candidates.size()];
            const auto shine = slots[static_cast<unsigned>(sms_msl_rand()) % slots.size()];
            result.rewards[shine] = reward;
        }
        if (failed)
            continue;
        const auto reach = result.reachable(available);
        bool complete = reach.unlocks == 0x7fffff;
        for (std::uint8_t id : {6, 16, 26, 36, 46, 56, 66})
            complete &= reach.shines.count(id) != 0;
        for (const auto& route : result.entrances.routes)
            complete &= reach.scenes.count(route.second) != 0;
        if (complete)
            return result;
    }
    throw std::runtime_error("No complete progressive placement found");
}

} // namespace sms_randomizer
