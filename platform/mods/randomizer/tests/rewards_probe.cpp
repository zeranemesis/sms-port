#include "../rewards.h"
#include <cstdio>

int main()
{
    for (unsigned scenario = 0; scenario < 128; ++scenario) {
        std::array<std::uint32_t, 120> rewards{};
        std::array<bool, 120> collected{};
        for (unsigned i = 0; i < rewards.size(); ++i) {
            rewards[i] = 1u << (i % 23);
            collected[i] = scenario < 120 ? i == scenario : (i + scenario) % 3 == 0;
        }
        const std::uint32_t initial = scenario % 2 ? 0 : 0x400;
        std::printf("%u %u\n", scenario, sms_randomizer::collected_unlocks(rewards, collected, initial));
    }
}
