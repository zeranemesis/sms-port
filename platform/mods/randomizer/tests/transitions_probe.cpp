#include "../transitions.h"
#include <cstdio>

int main()
{
    for (unsigned count : {0u, 1u, 128u}) {
        sms_randomizer::Transitions table;
        for (unsigned i = 0; i < count; ++i) {
            const auto key = static_cast<std::uint16_t>((i % 31) * 17);
            table.exits.emplace_back(key, static_cast<std::uint16_t>(0x200 + i * 13));
            table.level_indices.emplace_back(key, static_cast<std::uint16_t>(i));
        }
        for (unsigned k = 0; k < 64; ++k) {
            const auto scene = static_cast<std::uint16_t>(k % 2 ? 65535 - k : ((k / 2) % 31) * 17);
            std::printf("%u %u %u %u\n", count, scene, table.swap_exit(scene), table.level_index(scene));
        }
    }
}
