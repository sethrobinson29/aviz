#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "raylib.h"
#include "visuals/visual_module.h"

enum class VisualType { Waveform, Spectrum, Radial, Particles };

enum class LayerBlendMode { Normal, Additive, Multiply, Screen };

struct Layer {
    VisualType visual_type;
    std::unique_ptr<VisualModule> visual;
    float opacity = 1.0f;
    LayerBlendMode blend_mode = LayerBlendMode::Normal;
    Color tint_color = WHITE;
    std::optional<std::size_t> freq_band;
    ReactivitySource reactivity_source = ReactivitySource::GlobalBeat;
    float sensitivity = 1.0f;
    bool enabled = true;
};

std::unique_ptr<VisualModule> make_visual(VisualType type);
