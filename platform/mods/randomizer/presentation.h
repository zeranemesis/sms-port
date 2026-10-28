#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include "localization.h"

namespace sms_randomizer {

inline std::array<std::uint8_t, 3> rainbow_color(float hue)
{
    // Original HSL routines 0x5ed4/0x5f6c, saturation 1 and lightness .55.
    const float q = std::fma(-0.55f, 1.0f, 0.55f + 1.0f);
    const float p = std::fma(0.55f, 2.0f, -q);
    const auto channel = [=](float t) {
        if (t < 0) t += 1.0f;
        if (t > 1) t -= 1.0f;
        float result;
        if (static_cast<double>(t) < 1.0 / 6.0)
            result = std::fma((q - p) * 6.0f, t, p);
        else if (t < 0.5f) result = q;
        else if (static_cast<double>(t) < 2.0 / 3.0)
            result = static_cast<float>(std::fma(static_cast<double>(q - p) * 6.0,
                                               2.0 / 3.0 - static_cast<double>(t), static_cast<double>(p)));
        else result = p;
        return static_cast<std::uint8_t>(result * 255.0f);
    };
    return {channel(static_cast<float>(static_cast<double>(hue) + 1.0 / 3.0)),
            channel(hue), channel(static_cast<float>(static_cast<double>(hue) - 1.0 / 3.0))};
}

} // namespace sms_randomizer
