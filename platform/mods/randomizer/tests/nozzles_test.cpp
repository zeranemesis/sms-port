#include "../nozzles.h"
#include <cassert>

int main()
{
    // Bit order: spray, hover, rocket, turbo. Expected decisions follow the
    // original callback's branches, including Hover > Rocket > Turbo priority.
    constexpr sms_randomizer::Nozzles expected[] = {
        {-1, -1, true}, {-1, 0, false}, {4, -1, false}, {4, 0, false},
        {1, -1, false}, {1, 0, false}, {4, -1, false}, {4, 0, false},
        {5, -1, false}, {5, 0, false}, {4, -1, false}, {4, 0, false},
        {1, -1, false}, {1, 0, false}, {4, -1, false}, {4, 0, false},
    };
    for (unsigned bits = 0; bits < 16; ++bits) {
        const auto actual = sms_randomizer::select_nozzles(bits & 1, bits & 2, bits & 4, bits & 8);
        assert(actual.backup == expected[bits].backup);
        assert(actual.primary == expected[bits].primary);
        assert(actual.remove_fludd == expected[bits].remove_fludd);
    }
}
