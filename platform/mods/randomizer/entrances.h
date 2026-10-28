#pragma once

#include "start.h"
#include <array>
#include <map>
#include <set>
#include <stdexcept>

namespace sms_randomizer {

// Native preview format 1: all abilities unlocked; seeds are not interchangeable
// with the console generator. Keep this separate from its progressively ported algorithm.
struct EntranceSeed {
    std::map<std::uint16_t, std::uint16_t> routes;

    std::uint16_t destination(std::uint16_t scene) const
    {
        const auto found = routes.find(scene);
        return found == routes.end() ? scene : found->second;
    }

    bool completable(const std::vector<std::uint16_t>& scenes) const
    {
        std::set<std::uint16_t> visited{destination(0)};
        std::set<std::uint8_t> shines;
        bool changed = true;
        while (changed) {
            changed = false;
            const std::vector<std::uint8_t> ids(shines.begin(), shines.end());
            const auto capabilities = access_mask(0x7fffff, ids);
            for (const auto& rule : level_rules) {
                if (!visited.count(rule.scene) || (rule.requirements & ~capabilities))
                    continue;
                if (rule.kind == RuleKind::Shine && (rule.target & 0xff) <= shines.size()) {
                    changed |= shines.insert(static_cast<std::uint8_t>(rule.target >> 8)).second;
                    // A Shine returns the player to the ordinary plaza.
                    if (std::find(scenes.begin(), scenes.end(), 0x0108) != scenes.end())
                        changed |= visited.insert(0x0108).second;
                } else if (rule.kind == RuleKind::Exit && rule.flags <= shines.size()) {
                    const auto next = destination(rule.target);
                    if (std::find(scenes.begin(), scenes.end(), next) != scenes.end())
                        changed |= visited.insert(next).second;
                }
            }
        }
        for (std::uint8_t id : {6, 16, 26, 36, 46, 56, 66})
            if (!shines.count(id))
                return false;
        for (const auto& route : routes)
            if (!visited.count(route.second))
                return false;
        return true;
    }
};

inline EntranceSeed make_entrance_seed(std::uint32_t seed, const std::vector<std::uint16_t>& available)
{
    constexpr std::uint16_t fixed[] = {
        0x0100, 0x0101, 0x0102, 0x0105, 0x0106, 0x0107, 0x0108, 0x0109, 0x0f00, 0x1400, 0x3400, 0x3c00,
    };
    std::set<std::uint16_t> unique;
    for (const auto& rule : level_rules) {
        unique.insert(rule.scene);
        if (rule.kind == RuleKind::Exit)
            unique.insert(rule.target);
    }
    std::vector<std::uint16_t> scenes;
    for (const auto scene : unique)
        if (std::find(available.begin(), available.end(), scene) != available.end())
            scenes.push_back(scene);
    std::vector<std::uint16_t> sources;
    for (const auto scene : scenes)
        if (std::find(std::begin(fixed), std::end(fixed), scene) == std::end(fixed))
            sources.push_back(scene);
    if (std::find(sources.begin(), sources.end(), 0) == sources.end())
        throw std::runtime_error("Randomizer: missing Airstrip scene");
    for (unsigned attempt = 0; attempt < 256; ++attempt) {
        const auto first = starting_destination(seed + attempt * 0x9e3779b9u, 0x7fffff, sources);
        if (!first)
            throw std::runtime_error("Randomizer: no valid starting destination");
        std::vector<std::uint16_t> destinations = sources;
        destinations.erase(std::find(destinations.begin(), destinations.end(), *first));
        for (std::size_t i = destinations.size(); i > 1; --i)
            std::swap(destinations[i - 1], destinations[static_cast<unsigned>(sms_msl_rand()) % i]);
        EntranceSeed result;
        result.routes.emplace(0, *first);
        std::size_t index = 0;
        for (const auto source : sources)
            if (source != 0)
                result.routes.emplace(source, destinations[index++]);
        if (result.completable(scenes))
            return result;
    }
    throw std::runtime_error("Randomizer: no completable entrance layout found");
}

} // namespace sms_randomizer
