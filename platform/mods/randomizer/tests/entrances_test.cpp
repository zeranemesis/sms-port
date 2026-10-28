#include "../entrances.h"
#include <cassert>
#include <cstdio>

int main()
{
    std::set<std::uint16_t> all;
    for (const auto& rule : sms_randomizer::level_rules) {
        all.insert(rule.scene);
        if (rule.kind == sms_randomizer::RuleKind::Exit)
            all.insert(rule.target);
    }
    const std::vector<std::uint16_t> scenes(all.begin(), all.end());
    for (unsigned seed = 0; seed < 64; ++seed) {
        const auto first = sms_randomizer::make_entrance_seed(seed, scenes);
        const auto again = sms_randomizer::make_entrance_seed(seed, scenes);
        assert(first.routes == again.routes);
        assert(first.completable(scenes));
        std::set<std::uint16_t> destinations;
        for (const auto& route : first.routes)
            assert(destinations.insert(route.second).second);
        assert(first.destination(0x0108) == 0x0108);
    }
    bool rejected = false;
    try { sms_randomizer::make_entrance_seed(1, {0}); }
    catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
    std::puts("64 deterministic, bijective, progression-checked entrance seeds passed");
}
