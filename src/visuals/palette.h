#pragma once

#include <cstddef>
#include <vector>

#include "raylib.h"

struct PaletteStop {
    float position = 0.0f;
    Color color = WHITE;
};

struct Palette {
    std::vector<PaletteStop> stops;
};

bool operator==(const PaletteStop& a, const PaletteStop& b);
bool operator==(const Palette& a, const Palette& b);
inline bool operator!=(const PaletteStop& a, const PaletteStop& b) { return !(a == b); }
inline bool operator!=(const Palette& a, const Palette& b) { return !(a == b); }

namespace palette_math {

Color sample(const Palette& palette, float t);

Color sample_by_index(const Palette& palette, std::size_t index, std::size_t count);

std::vector<Color> to_gradient_pixels(const Palette& palette, int width);

}
