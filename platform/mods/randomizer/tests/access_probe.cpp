#include "../access.h"
#include <cstdio>

int main()
{
    const std::vector<std::vector<std::uint8_t>> shines = {
        {}, {6, 16, 26, 36, 46, 56, 66}, {6, 16, 26, 36, 46, 56},
        {6, 16, 26, 36, 46, 56, 66, 6}, {6, 6, 6, 6, 6, 6, 6},
        {5, 7, 15, 17, 25, 27, 35}, {6, 16, 26, 36, 46, 56, 66, 0, 119},
    };
    std::vector<std::uint32_t> masks{0, 0x7fffff};
    for (unsigned i = 0; i < 23; ++i) {
        masks.push_back(1u << i);
        masks.push_back(0x7fffff ^ (1u << i));
        for (unsigned j = i + 1; j < 23; ++j)
            masks.push_back((1u << i) | (1u << j));
    }
    std::uint32_t random = 0x534d5352;
    for (unsigned i = 0; i < 4096; ++i) {
        random = random * 1664525u + 1013904223u;
        masks.push_back(random & 0x7fffff);
    }
    for (const auto mask : masks)
        for (unsigned pattern = 0; pattern < shines.size(); ++pattern)
            std::printf("%u %u %u\n", mask, pattern, sms_randomizer::access_mask(mask, shines[pattern]));
}
