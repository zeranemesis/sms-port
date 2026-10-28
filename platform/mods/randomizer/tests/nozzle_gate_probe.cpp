#include "../nozzles.h"
#include <cassert>
#include <cstdio>

int main()
{
    for (unsigned mask = 0; mask < 32; ++mask) {
        std::uint32_t unlocks = 0;
        constexpr unsigned bits[] = {17, 19, 20, 21, 14};
        for (unsigned bit = 0; bit < 5; ++bit)
            if (mask & (1u << bit)) unlocks |= 1u << bits[bit];
        for (unsigned nozzle = 0; nozzle < 6; ++nozzle)
            std::printf("%u %u %u %u\n", unlocks, nozzle,
                        sms_randomizer::can_use_nozzle(unlocks, nozzle),
                        sms_randomizer::can_switch_backup(unlocks));
    }
    assert(!sms_randomizer::can_use_nozzle(0x7fffff, 6));
    assert(!sms_randomizer::can_use_nozzle(0x7fffff, ~0u));
}
