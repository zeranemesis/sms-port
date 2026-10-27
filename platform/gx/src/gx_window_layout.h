#pragma once

#include <algorithm>
#include <cmath>

namespace gx {
struct WindowArea { int x, y, w, h; };
struct WindowBorders { int top, left, bottom, right; };

// Determine the initial CLIENT rectangle while keeping the entire decorated
// window visible. Input coordinates are SDL logical window coordinates.
// The requested game render scale never changes the usable desktop bounds.
inline WindowArea initialWindowLayout(WindowArea desktop, double aspect, int windowScale = 0,
                                      WindowBorders borders = {48, 8, 8, 8}) {
    if (!(aspect > 0.0) || !std::isfinite(aspect)) aspect = 4.0 / 3.0;

    const int frameWidth = borders.left + borders.right;
    const int frameHeight = borders.top + borders.bottom;
    const int usableWidth = std::max(1, static_cast<int>(desktop.w * 0.8) - frameWidth);
    const int usableHeight = std::max(1, static_cast<int>(desktop.h * 0.8) - frameHeight);

    // Default to a 720-line window; an explicit scale requests 480 lines
    // per step, subject in both cases to the actual available screen area.
    const double requestedHeight = windowScale > 0 ? 480.0 * windowScale : 720.0;
    const double defaultWidthCeiling = windowScale > 0 ? static_cast<double>(usableWidth)
                                                       : std::min(1280.0, static_cast<double>(usableWidth));
    const double targetHeight = std::min({requestedHeight,
                                          static_cast<double>(usableHeight),
                                          defaultWidthCeiling / aspect});

    // Keep the selected aspect ratio until the final integer rounding step.
    const int height = std::max(1, static_cast<int>(std::lround(targetHeight)));
    const int width = std::max(1, std::min(usableWidth,
                               static_cast<int>(std::lround(targetHeight * aspect))));
    const int outerWidth = width + frameWidth;
    const int outerHeight = height + frameHeight;

    return {desktop.x + (desktop.w - outerWidth) / 2 + borders.left,
            desktop.y + (desktop.h - outerHeight) / 2 + borders.top,
            width, height};
}
}  // namespace gx
