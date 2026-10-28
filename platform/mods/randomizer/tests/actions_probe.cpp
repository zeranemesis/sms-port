#include "../actions.h"
#include <cstdio>
#include <vector>

int main()
{
    std::vector<std::uint32_t> masks{0, 0x400, 0x7fffff};
    for (unsigned bit = 0; bit < 23; ++bit) {
        masks.push_back(1u << bit);
        masks.push_back(0x7fffff ^ (1u << bit));
    }
    std::uint32_t random = 0x534d5352;
    for (unsigned i = 0; i < 1024; ++i) {
        random = random * 1664525u + 1013904223u;
        masks.push_back(random & 0x7fffff);
    }
    for (const auto mask : masks)
        for (const auto status : {0x00810446u, 0x383u, 0x384u, 0x8a7u, 0x895u,
                                 0x896u, 0x441u, 0x887u, 0x02000881u, 0x008008a9u,
                                 0x882u, 0x00200346u, 0x00200349u, 0x08200348u,
                                 0x3000036cu, 0x3000036bu, 0x10100341u, 0x02000889u,
                                 0x02000880u, 0x891u, 0x02000886u})
            for (const bool player : {false, true}) {
                const auto result = sms_randomizer::filter_action(mask, status, player);
                std::printf("%u %u %u %u %u %u\n", mask, status, player,
                            result.status, result.allowed, result.clear_holder);
            }
}
