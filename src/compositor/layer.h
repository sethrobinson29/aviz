#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "raylib.h"
#include "visuals/palette.h"
#include "visuals/visual_module.h"

enum class VisualType { Waveform, Spectrum, Radial, Particles, Lissajous, FlowField, Fractal };

enum class LayerBlendMode { Normal, Additive, Multiply, Screen };

inline constexpr std::uint64_t kInvalidLayerId = 0;

struct Layer {
    VisualType visual_type;
    std::unique_ptr<VisualModule> visual;
    float opacity = 1.0f;
    LayerBlendMode blend_mode = LayerBlendMode::Normal;
    Color tint_color = WHITE;
    std::optional<Palette> palette_override;
    std::optional<std::size_t> freq_band;
    ReactivitySource reactivity_source = ReactivitySource::GlobalBeat;
    float sensitivity = 1.0f;
    bool enabled = true;
    std::uint64_t layer_id = kInvalidLayerId;
};

std::unique_ptr<VisualModule> make_visual(VisualType type);
