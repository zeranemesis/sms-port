#include "../candidates.h"
#include <cstdio>

int main()
{
    const std::vector<std::uint8_t> shines{6, 16, 26, 36, 46, 56, 66};
    const std::uint32_t owned[] = {0, 1, 4, 0x80000, 0x80001, 0x80004, 0x22, 0x7fffff};
    for (unsigned required = 0; required < 0x20000; required += 127) {
        for (const auto mask : owned) {
            const auto candidates = sms_randomizer::candidate_unlocks(required, mask, shines);
            std::printf("%u %u", required, mask);
            for (const auto value : candidates)
                std::printf(" %u", value);
            std::putchar('\n');
        }
    }
}
