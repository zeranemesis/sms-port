#include "../progression.h"
#include "../rewards.h"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

int main(int argc, char** argv)
{
    assert(argc <= 2);
    std::set<std::uint16_t> unique;
    for (const auto& rule : sms_randomizer::level_rules) {
        unique.insert(rule.scene);
        if (rule.kind == sms_randomizer::RuleKind::Exit)
            unique.insert(rule.target);
    }
    if (argc == 2) {
        std::ifstream manifest(argv[1]);
        std::string header;
        assert(std::getline(manifest, header) && header == "Sunshine native entrance preview v1");
        assert(std::getline(manifest, header) && header.find("seed ") == 0);
        unique.clear();
        // These are the destinations actually enumerated in the native session,
        // plus the fixed plaza used for the game's Shine return.
        unique.insert(0x0108);
        unsigned source, destination;
        while (manifest >> source >> destination) {
            assert(source <= 0xffff && destination <= 0xffff);
            unique.insert(static_cast<std::uint16_t>(source));
            unique.insert(static_cast<std::uint16_t>(destination));
        }
        assert(manifest.eof() && unique.size() > 1);
    }
    const std::vector<std::uint16_t> available(unique.begin(), unique.end());
    for (unsigned seed = 0; seed < 256; ++seed) {
        const auto first = sms_randomizer::make_progressive_seed(seed, available);
        const auto again = sms_randomizer::make_progressive_seed(seed, available);
        assert(first.entrances.routes == again.entrances.routes);
        assert(first.rewards == again.rewards);
        std::uint32_t placed = 0;
        unsigned count = 0;
        for (const auto reward : first.rewards) {
            if (!reward) continue;
            assert((reward & (reward - 1)) == 0);
            assert(!(placed & reward));
            assert(!(reward & 0x400));
            placed |= reward;
            ++count;
        }
        assert(count == 22 && (placed | 0x400) == 0x7fffff);
        const auto reach = first.reachable(available);
        assert(reach.unlocks == 0x7fffff);
        std::array<bool, 120> collected{};
        for (const auto shine : reach.shines)
            collected[shine] = true;
        assert(sms_randomizer::collected_unlocks(first.rewards, collected, 0x400) == reach.unlocks);
        collected.fill(false);
        assert(sms_randomizer::collected_unlocks(first.rewards, collected, 0x400) == 0x400);
        std::uint32_t restored = 0x400;
        for (unsigned shine = 0; shine < collected.size(); ++shine) {
            collected[shine] = true;
            restored |= first.rewards[shine];
            assert(sms_randomizer::collected_unlocks(first.rewards, collected, 0x400) == restored);
        }
    }
    bool rejected = false;
    try { sms_randomizer::make_progressive_seed(1, {}); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    std::printf("256 progressive seeds, %zu available scenes: deterministic placements, 22 unique rewards, complete logical progression and incremental flag-based restoration passed\n", available.size());
}
