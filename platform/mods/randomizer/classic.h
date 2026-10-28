#pragma once

#include "start.h"
#include "candidates.h"
#include <array>
#include <stdexcept>
#include <utility>

namespace sms_randomizer {

struct ClassicSeed {
    std::vector<std::pair<std::uint16_t, std::uint16_t>> routes;
    std::array<std::uint32_t, 120> rewards{};
};

// Translation of original generation routine 0xcc. Preserve vector order and
// duplicate candidates: both affect subsequent MSL random draws.
inline ClassicSeed make_classic_seed(std::uint32_t seed, std::vector<std::uint16_t> scenes,
                                     bool start_with_all_unlocks = false)
{
    std::sort(scenes.begin(), scenes.end());
    if (scenes.size() < 110 || scenes.size() > 128 ||
        std::adjacent_find(scenes.begin(), scenes.end()) != scenes.end())
        throw std::runtime_error("Classic Randomizer requires the original scenes within its 128-entry archive capacity");
    constexpr std::uint16_t fixed[] = {
        0x100, 0x101, 0x102, 0x105, 0x106, 0x107, 0x108, 0x109, 0x3400, 0x3c00,
    };
    const auto available = [&](std::uint16_t scene) {
        return std::binary_search(scenes.begin(), scenes.end(), scene);
    };
    for (const auto scene : fixed)
        if (!available(scene))
            throw std::runtime_error("Classic Randomizer is missing an original fixed scene");
    if (!available(0xf00))
        throw std::runtime_error("Classic Randomizer is missing the fixed ending scene");
    for (const auto& rule : level_rules)
        if (!available(rule.scene) || (rule.kind == RuleKind::Exit && !available(rule.target)))
            throw std::runtime_error("Classic Randomizer is missing a required archive scene");
    ClassicSeed result;
    std::vector<std::uint16_t> assigned(std::begin(fixed), std::end(fixed));
    auto destinations = assigned;
    std::vector<Rule> open_exits, locked_exits, shines, locked_shines;
    std::uint32_t unlocks = start_with_all_unlocks ? 0x7fffff : 0x400;
    bool final_exits_added = false;
    const auto contains = [](const auto& values, auto value) {
        return std::find(values.begin(), values.end(), value) != values.end();
    };
    const auto shine_ids = [&]() {
        std::vector<std::uint8_t> ids;
        for (const auto& shine : shines)
            ids.push_back(static_cast<std::uint8_t>(shine.target >> 8));
        return ids;
    };
    const auto capabilities = [&]() { return access_mask(unlocks, shine_ids()); };
    const auto promote_exits = [&]() {
        const auto access = capabilities();
        for (auto it = locked_exits.begin(); it != locked_exits.end();) {
            if (!(it->requirements & ~access) && it->flags <= shines.size()) {
                open_exits.push_back(*it);
                it = locked_exits.erase(it);
            } else ++it;
        }
    };
    const auto promote_shines = [&]() {
        const auto access = capabilities();
        for (auto it = locked_shines.begin(); it != locked_shines.end();) {
            if (!(it->requirements & ~access) && (it->target & 255) <= shines.size()) {
                shines.push_back(*it);
                it = locked_shines.erase(it);
            } else ++it;
        }
    };
    const auto pool = [&](bool shines_only, bool exits_only) {
        std::vector<std::uint16_t> choices;
        const auto access = capabilities();
        for (const auto scene : scenes) {
            if ((scene == 0x1400 || scene == 0x3400) && !(access & 0x10000))
                continue;
            if (!contains(destinations, scene) &&
                level_accessible(scene, access, static_cast<unsigned>(shines.size()), shines_only, exits_only))
                choices.push_back(scene);
        }
        return choices;
    };
    const auto draw = [](const auto& values) {
        if (values.empty()) throw std::runtime_error("Original generation reached an empty draw pool");
        return values[static_cast<unsigned>(sms_msl_rand()) % values.size()];
    };
    const auto missing_unlocks = [&]() {
        std::vector<std::uint32_t> missing;
        for (unsigned bit = 0; bit < 23; ++bit)
            if (!(unlocks & (1u << bit))) missing.push_back(1u << bit);
        return missing;
    };
    const auto add_level = [&](std::uint16_t scene, std::uint32_t inherited) {
        const auto access = capabilities();
        for (const auto& original : level_rules) {
            if (original.scene != scene) continue;
            auto rule = original;
            rule.requirements |= inherited;
            if (rule.kind == RuleKind::Exit)
                (rule.requirements & ~access ? locked_exits : open_exits).push_back(rule);
            else
                (rule.requirements & ~access ? locked_shines : shines).push_back(rule);
        }
    };
    sms_msl_srand(seed);
    for (const auto scene : fixed)
        result.routes.emplace_back(scene, scene);
    for (const auto& rule : level_rules)
        if (rule.scene == 0x108)
            (rule.kind == RuleKind::Exit ? locked_exits : locked_shines).push_back(rule);
    result.routes.emplace_back(0xf00, 0xf00);
    const auto first = draw(pool(true, false));
    assigned.push_back(0);
    destinations.push_back(first);
    result.routes.emplace_back(0, first);
    add_level(first, 0);
    promote_exits();
    promote_shines();
    for (unsigned iteration = 0; !open_exits.empty() || !locked_exits.empty(); ++iteration) {
        if (iteration == 4096) throw std::runtime_error("Classic generation iteration limit reached");
        promote_exits();
        promote_shines();
        auto choices = pool(false, open_exits.size() + locked_exits.size() < 2);
        if (choices.empty()) choices = pool(false, false);
        if (!open_exits.empty() && !choices.empty()) {
            const auto exit = open_exits.front();
            open_exits.erase(open_exits.begin());
            if (contains(assigned, exit.target)) continue;
            auto destination = exit.target;
            if (std::find(std::begin(fixed), std::end(fixed), exit.target) == std::end(fixed)) {
                const bool need_shine = shines.size() < 3;
                auto options = pool(need_shine, open_exits.size() + locked_exits.size() < 11);
                if (options.empty()) options = pool(need_shine, false);
                destination = draw(options);
            }
            assigned.push_back(exit.target);
            destinations.push_back(destination);
            result.routes.emplace_back(exit.target, destination);
            add_level(destination, exit.requirements);
            if (!final_exits_added && (capabilities() & 0x10000)) {
                locked_exits.push_back({0, RuleKind::Exit, 0x3400, 0, 0x10008});
                locked_exits.push_back({0, RuleKind::Exit, 0x1400, 0, 0x10100});
                promote_exits();
                final_exits_added = true;
            }
        } else {
            const auto access = capabilities();
            // The original 17-byte arrays overlap at their final byte. In
            // particular, the seven-Shadow-Mario bit also sets single[0].
            std::array<std::uint8_t, 49> counts{};
            counts[16] = static_cast<std::uint8_t>((access >> 16) & 1);
            for (const auto& exit : locked_exits) {
                const bool has_shine = std::any_of(std::begin(level_rules), std::end(level_rules),
                    [&](const Rule& rule) { return rule.scene == exit.target && rule.kind == RuleKind::Shine; });
                if (!has_shine) continue;
                const auto needed = exit.requirements & ~access;
                unsigned count = 0, last = 0;
                for (unsigned bit = 0; bit < 17; ++bit)
                    if (needed & (1u << bit)) { ++counts[32 + bit]; ++count; last = bit; }
                if (count == 1) ++counts[16 + last];
            }
            std::vector<std::uint32_t> candidates;
            for (const auto offset : {16u, 32u}) {
                if (!candidates.empty()) break;
                for (unsigned bit = 0; bit < 17; ++bit)
                    if (counts[offset + bit]) {
                        const auto extra = candidate_unlocks(1u << bit, unlocks, shine_ids());
                        candidates.insert(candidates.end(), extra.begin(), extra.end());
                    }
            }
            if (candidates.empty()) candidates = missing_unlocks();
            if (candidates.empty()) throw std::runtime_error("Original generation has no remaining ability");
            std::vector<std::uint8_t> slots;
            for (const auto id : shine_ids())
                if (!result.rewards[id]) slots.push_back(id);
            if (slots.empty()) throw std::runtime_error("Original generation has no free accessible Shine");
            const auto reward = draw(candidates);
            const auto shine = draw(slots);
            result.rewards[shine] = reward;
            unlocks |= reward;
            promote_exits();
            promote_shines();
        }
    }
    for (auto missing = missing_unlocks(); !missing.empty(); missing = missing_unlocks()) {
        const auto reward = draw(missing);
        std::uint8_t shine;
        do { shine = draw(shine_ids()); } while (result.rewards[shine]);
        result.rewards[shine] = reward;
        unlocks |= reward;
    }
    if (result.routes.size() != 110)
        throw std::runtime_error("Classic generation did not produce the original 110 routes");
    return result;
}

} // namespace sms_randomizer
