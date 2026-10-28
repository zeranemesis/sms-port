#include "../presentation.h"
#include <cassert>
#include <cstdio>
#include <cstring>

int main()
{
    assert(!sms_randomizer::unlock_name(0));
    assert(!sms_randomizer::unlock_name(3));
    assert(!sms_randomizer::unlock_name(1u << 23));
    for (unsigned bit = 0; bit < 23; ++bit) assert(sms_randomizer::unlock_name(1u << bit));
    assert(std::strcmp(sms_randomizer::unlock_name(1u << 19, 2), "BUSE STATIONNAIRE") == 0);
    for (unsigned step = 0; step <= 1024; ++step) {
        const float hue = step / 1024.0f;
        const auto color = sms_randomizer::rainbow_color(hue);
        std::uint32_t bits;
        std::memcpy(&bits, &hue, sizeof(bits));
        std::printf("%u %u %u %u\n", bits, color[0], color[1], color[2]);
    }
}
