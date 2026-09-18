#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "compositor/layer.h"

namespace compositor_math {

float clamp_opacity(float opacity);

bool has_duplicate_layer_ids(const std::vector<Layer>& layers);
bool has_invalid_layer_id(const std::vector<Layer>& layers);
std::uint64_t next_layer_id_after_load(const std::vector<Layer>& layers);

inline constexpr float kSensitivityMin = 0.0f;
inline constexpr float kSensitivityMax = 4.0f;
float clamp_sensitivity(float sensitivity);

std::optional<std::size_t> next_freq_band(std::optional<std::size_t> current,
                                           std::size_t num_bands);
std::optional<std::size_t> prev_freq_band(std::optional<std::size_t> current,
                                           std::size_t num_bands);

ReactivitySource default_reactivity_source(std::optional<std::size_t> freq_band);

ReactivitySource next_reactivity_source(ReactivitySource current,
                                         std::optional<std::size_t> freq_band);
ReactivitySource prev_reactivity_source(ReactivitySource current,
                                         std::optional<std::size_t> freq_band);

struct BlendFactors {
    int raylib_mode;
    int src_factor;
    int dst_factor;
    int equation;
};
BlendFactors to_blend_factors(LayerBlendMode mode);

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

}
