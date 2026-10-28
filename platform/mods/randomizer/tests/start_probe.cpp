#include "../start.h"
#include <cassert>
#include <cstdio>
#include <set>

int main()
{
    assert(!sms_randomizer::starting_destination(1, 0x400, {}));
    assert(!sms_randomizer::starting_destination(1, 0x400, {0x0108, 0x1400, 0xffff}));
    std::set<std::uint16_t> unique;
    for (const auto& rule : sms_randomizer::level_rules)
        unique.insert(rule.scene);
    const std::vector<std::uint16_t> scenes(unique.begin(), unique.end());
    for (std::uint32_t unlocks : {0u, 0x400u, 0x7fffffu}) {
        const auto pool = sms_randomizer::starting_destinations(unlocks, scenes);
        std::printf("pool %u", unlocks);
        for (auto scene : pool)
            std::printf(" %u", scene);
        std::putchar('\n');
        for (std::uint32_t seed = 0; seed < 256; ++seed) {
            const auto chosen = sms_randomizer::starting_destination(seed, unlocks, scenes);
            std::printf("seed %u %u %d\n", unlocks, seed, chosen ? static_cast<int>(*chosen) : -1);
        }
    }
}
