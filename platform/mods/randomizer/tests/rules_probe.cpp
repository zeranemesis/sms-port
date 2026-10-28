#include "../rules.h"
#include <cstdio>

int main()
{
    for (const auto& rule : sms_randomizer::level_rules)
        std::printf("%u %u %u %u %u\n", rule.scene, static_cast<unsigned>(rule.kind),
                    rule.target, rule.flags, rule.requirements);
}
