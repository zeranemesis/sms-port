#include "../eligibility.h"
#include <cstdio>
#include <set>

int main()
{
    std::set<std::uint16_t> scenes{0xffff};
    for (const auto& rule : sms_randomizer::level_rules)
        scenes.insert(rule.scene);
    const std::uint32_t masks[] = {0, 1, 0x100, 0x8000, 0x10000, 0x1ffff};
    for (const auto scene : scenes)
        for (const auto mask : masks)
            for (unsigned shines : {0u, 1u, 10u, 120u})
                for (unsigned flags = 0; flags < 4; ++flags)
                    std::printf("%u %u %u %u %u\n", scene, mask, shines, flags,
                        sms_randomizer::level_accessible(scene, mask, shines, flags & 1, flags & 2));
}
