#include "visuals/palette.h"

#include <algorithm>
#include <cassert>
#include <cstddef>

bool operator==(const PaletteStop& a, const PaletteStop& b) {
    return a.position == b.position && a.color.r == b.color.r && a.color.g == b.color.g &&
           a.color.b == b.color.b && a.color.a == b.color.a;
}

bool operator==(const Palette& a, const Palette& b) { return a.stops == b.stops; }

namespace palette_math {

Color sample(const Palette& palette, float t) {
    assert(!palette.stops.empty());
    assert(std::is_sorted(palette.stops.begin(), palette.stops.end(),
                           [](const PaletteStop& a, const PaletteStop& b) { return a.position < b.position; }));

    if (palette.stops.size() == 1) {
        return palette.stops.front().color;
    }

    t = std::clamp(t, palette.stops.front().position, palette.stops.back().position);

    std::size_t upper = 0;
    while (upper < palette.stops.size() - 1 && palette.stops[upper].position < t) {
        ++upper;
    }
    if (upper == 0) {
        return palette.stops.front().color;
    }

    const PaletteStop& lo = palette.stops[upper - 1];
    const PaletteStop& hi = palette.stops[upper];
    const float span = hi.position - lo.position;
    const float local_t = span > 0.0f ? (t - lo.position) / span : 0.0f;

    auto lerp_channel = [local_t](unsigned char a, unsigned char b) {
        return static_cast<unsigned char>(static_cast<float>(a) +
                                           (static_cast<float>(b) - static_cast<float>(a)) * local_t);
    };
    return Color{lerp_channel(lo.color.r, hi.color.r), lerp_channel(lo.color.g, hi.color.g),
                 lerp_channel(lo.color.b, hi.color.b), lerp_channel(lo.color.a, hi.color.a)};
}

Color sample_by_index(const Palette& palette, std::size_t index, std::size_t count) {
    const float t = count > 1 ? static_cast<float>(index) / static_cast<float>(count - 1) : 0.0f;
    return sample(palette, t);
}

std::vector<Color> to_gradient_pixels(const Palette& palette, int width) {
    const std::size_t count = static_cast<std::size_t>(std::max(width, 1));
    std::vector<Color> pixels;
    pixels.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        pixels.push_back(sample_by_index(palette, i, count));
    }
    return pixels;
}

}
