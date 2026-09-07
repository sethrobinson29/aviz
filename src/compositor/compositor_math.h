#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <vector>

#include "compositor/layer.h"

namespace compositor_math {

// Range: [0, 1].
float clamp_opacity(float opacity);

// Multiplier applied to a layer's reactivity strength (amplitude scaling,
// onset pulse magnitude). 1.0 = unmodified.
inline constexpr float kSensitivityMin = 0.0f;
inline constexpr float kSensitivityMax = 4.0f;
float clamp_sensitivity(float sensitivity);

// Cycles a layer's freq_band through band indices [0, num_bands) and
// Global (nullopt): Global -> 0 -> 1 -> ... -> num_bands-1 -> Global for
// next_freq_band, the reverse for prev_freq_band.
std::optional<std::size_t> next_freq_band(std::optional<std::size_t> current,
                                           std::size_t num_bands);
std::optional<std::size_t> prev_freq_band(std::optional<std::size_t> current,
                                           std::size_t num_bands);

// The reactivity_source a layer should fall back to when freq_band changes
// mode (Global <-> a specific band), since band_onset/band_amplitude only
// apply to a band and global_beat is the only valid source for Global.
ReactivitySource default_reactivity_source(std::optional<std::size_t> freq_band);

// For a band (freq_band has a value), cycles reactivity_source within the
// 2-option pair {BandOnset, BandAmplitude}; if `current` isn't one of those
// two (stale from a prior mode), returns default_reactivity_source(freq_band).
// For Global, GlobalBeat is the only valid source, so both always return it
// -- a no-op toggle, not a 2-option cycle.
ReactivitySource next_reactivity_source(ReactivitySource current,
                                         std::optional<std::size_t> freq_band);
ReactivitySource prev_reactivity_source(ReactivitySource current,
                                         std::optional<std::size_t> freq_band);

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
