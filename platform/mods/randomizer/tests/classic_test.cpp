#include "../classic.h"
#include <cassert>
#include <cstdio>
#include <set>

int main()
{
    std::set<std::uint16_t> unique{0x100, 0x101, 0x102, 0x105, 0x106, 0x107, 0x108,
                                 0x109, 0xf00, 0x3400, 0x3c00};
    for (const auto& rule : sms_randomizer::level_rules) {
        unique.insert(rule.scene);
        if (rule.kind == sms_randomizer::RuleKind::Exit) unique.insert(rule.target);
    }
    const std::vector<std::uint16_t> scenes(unique.begin(), unique.end());
    // FNV-1a over big-endian route pairs followed by the 120 uint32 rewards.
    // These three fingerprints come from the original PPC generator oracle.
    constexpr std::pair<std::uint32_t, std::uint64_t> expected[] = {
        {0, 0x5534629975681d22ull}, {1, 0xcf7b1f35831bb626ull},
        {0xffffffff, 0x8a1b7e2853f4a2aeull},
    };
    for (const auto& fixture : expected) {
        const auto result = sms_randomizer::make_classic_seed(fixture.first, scenes);
        std::uint64_t hash = 0xcbf29ce484222325ull;
        const auto append = [&](std::uint32_t value, unsigned bytes) {
            for (int shift = static_cast<int>(bytes * 8) - 8; shift >= 0; shift -= 8) {
                hash ^= (value >> shift) & 255;
                hash *= 0x100000001b3ull;
            }
        };
        for (const auto& route : result.routes) {
            append(route.first, 2);
            append(route.second, 2);
        }
        for (const auto reward : result.rewards) append(reward, 4);
        assert(hash == fixture.second);
    }
    auto pal = scenes;
    pal.push_back(0x0c01); // Existing unused PAL archive; the original generator ignores it.
    const auto ordinary = sms_randomizer::make_classic_seed(1, scenes);
    const auto with_unused = sms_randomizer::make_classic_seed(1, pal);
    assert(ordinary.routes == with_unused.routes && ordinary.rewards == with_unused.rewards);
    for (unsigned kind = 0; kind < 4; ++kind) {
        auto invalid = scenes;
        if (kind == 0) invalid.clear();
        if (kind == 1) invalid.back() = invalid.front();
        if (kind == 2) invalid.back() = 0xffff;
        if (kind == 3) invalid.erase(invalid.begin());
        bool rejected = false;
        try { sms_randomizer::make_classic_seed(1, invalid); }
        catch (const std::runtime_error&) { rejected = true; }
        assert(rejected);
    }
    std::puts("Three original complete-seed fingerprints, unused PAL archive and four invalid archive fixtures passed");
}
