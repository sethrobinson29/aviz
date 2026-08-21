#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "compositor/layer.h"

namespace compositor_math {

// Range: [0, 1].
float clamp_opacity(float opacity);

// GL blend factors for a compositor-stage LayerBlendMode. `raylib_mode` is
// the value to pass to BeginBlendMode(). For LayerBlendMode::Screen this is
// BLEND_CUSTOM, and `src_factor`/`dst_factor`/`equation` (raw rlgl RL_*
// constants) must additionally be applied via rlSetBlendFactors() before
// the BeginBlendMode(BLEND_CUSTOM) call takes effect — for every other mode
// these three fields are unused (raylib's built-in modes already encode
// the right GL factors internally).
struct BlendFactors {
    int raylib_mode;
    int src_factor;
    int dst_factor;
    int equation;
};
BlendFactors to_blend_factors(LayerBlendMode mode);

// Indices are clamped into [0, v.size()); no-op on an empty vector.
template <typename T>
void move_element(std::vector<T>& v, std::size_t from, std::size_t to) {
    if (v.empty()) {
        return;
    }
    from = std::min(from, v.size() - 1);
    to = std::min(to, v.size() - 1);
    if (from == to) {
        return;
    }
    if (from < to) {
        std::rotate(v.begin() + from, v.begin() + from + 1, v.begin() + to + 1);
    } else {
        std::rotate(v.begin() + to, v.begin() + from, v.begin() + from + 1);
    }
}

}  // namespace compositor_math
