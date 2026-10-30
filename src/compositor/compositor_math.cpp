#include "compositor/compositor_math.h"

#include <limits>
#include <unordered_set>

#include "compositor/compositor.h"
#include "raylib.h"
#include "rlgl.h"
#include "visuals/number_clamp.h"

namespace compositor_math {

float clamp_opacity(double opacity) { return number_math::clamp_number_to_range<float>(opacity, 0.0f, 1.0f); }

Color layer_palette_color(std::size_t index) {
    float hue = (360.0f * static_cast<float>(index)) / static_cast<float>(Compositor::kMaxLayers);
    return ColorFromHSV(hue, kLayerPaletteSaturation, kLayerPaletteValue);
}

void reassign_layer_tints(std::vector<Layer>& layers) {
    for (std::size_t i = 0; i < layers.size(); ++i) {
        layers[i].tint_color = layer_palette_color(i);
    }
}

Color layer_composite_color(const Layer& layer) {
    const Color tint = layer.palette_override.has_value() ? WHITE : layer.tint_color;
    return Color{tint.r, tint.g, tint.b, static_cast<unsigned char>(clamp_opacity(layer.opacity) * 255.0f)};
}

bool has_duplicate_layer_ids(const std::vector<Layer>& layers) {
    std::unordered_set<std::uint64_t> seen;
    seen.reserve(layers.size());
    for (const Layer& layer : layers) {
        if (!seen.insert(layer.layer_id).second) {
            return true;
        }
    }
    return false;
}

bool has_invalid_layer_id(const std::vector<Layer>& layers) {
    for (const Layer& layer : layers) {
        if (layer.layer_id == kInvalidLayerId) {
            return true;
        }
    }
    return false;
}

std::uint64_t next_layer_id_after_load(const std::vector<Layer>& layers) {
    if (layers.empty()) {
        return 1;
    }
    std::uint64_t max_id = 0;
    for (const Layer& layer : layers) {
        max_id = std::max(max_id, layer.layer_id);
    }
    return max_id == std::numeric_limits<std::uint64_t>::max() ? max_id : max_id + 1;
}

float clamp_sensitivity(double sensitivity) {
    return number_math::clamp_number_to_range<float>(sensitivity, kSensitivityMin, kSensitivityMax);
}

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
    return ReactivitySource::GlobalBeat;
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
            return BlendFactors{BLEND_CUSTOM, RL_ONE_MINUS_DST_COLOR, RL_ONE, RL_FUNC_ADD};
    }
    return BlendFactors{BLEND_ALPHA, 0, 0, 0};
}

}
