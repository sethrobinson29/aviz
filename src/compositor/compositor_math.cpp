#include "compositor/compositor_math.h"

#include "raylib.h"
#include "rlgl.h"

namespace compositor_math {

float clamp_opacity(float opacity) { return std::clamp(opacity, 0.0f, 1.0f); }

std::optional<std::size_t> next_freq_band(std::optional<std::size_t> current,
                                           std::size_t num_bands) {
    if (!current.has_value()) {
        return num_bands > 0 ? std::optional<std::size_t>(0) : std::nullopt;
    }
    if (*current + 1 >= num_bands) {
        return std::nullopt;
    }
    return *current + 1;
}

std::optional<std::size_t> prev_freq_band(std::optional<std::size_t> current,
                                           std::size_t num_bands) {
    if (!current.has_value()) {
        return num_bands > 0 ? std::optional<std::size_t>(num_bands - 1) : std::nullopt;
    }
    if (*current == 0) {
        return std::nullopt;
    }
    return *current - 1;
}

ReactivitySource default_reactivity_source(std::optional<std::size_t> freq_band) {
    return freq_band.has_value() ? ReactivitySource::BandOnset : ReactivitySource::GlobalBeat;
}

ReactivitySource next_reactivity_source(ReactivitySource current,
                                         std::optional<std::size_t> freq_band) {
    if (freq_band.has_value()) {
        return current == ReactivitySource::BandOnset ? ReactivitySource::BandAmplitude
                                                        : ReactivitySource::BandOnset;
    }
    return current == ReactivitySource::GlobalBeat ? ReactivitySource::PeakFreq
                                                     : ReactivitySource::GlobalBeat;
}

ReactivitySource prev_reactivity_source(ReactivitySource current,
                                         std::optional<std::size_t> freq_band) {
    return next_reactivity_source(current, freq_band);
}

BlendFactors to_blend_factors(LayerBlendMode mode) {
    switch (mode) {
        case LayerBlendMode::Normal:
            return BlendFactors{BLEND_ALPHA, 0, 0, 0};
        case LayerBlendMode::Additive:
            return BlendFactors{BLEND_ADDITIVE, 0, 0, 0};
        case LayerBlendMode::Multiply:
            return BlendFactors{BLEND_MULTIPLIED, 0, 0, 0};
        case LayerBlendMode::Screen:
            // result = src + dst - src*dst -- classic "screen" formula.
            return BlendFactors{BLEND_CUSTOM, RL_ONE_MINUS_DST_COLOR, RL_ONE, RL_FUNC_ADD};
    }
    return BlendFactors{BLEND_ALPHA, 0, 0, 0};
}

}  // namespace compositor_math
