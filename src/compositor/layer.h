#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "raylib.h"
#include "visuals/visual_module.h"

enum class VisualType { Waveform, Spectrum };

// Blend modes applied per layer when its offscreen texture is drawn onto
// the compositor's output texture.
enum class LayerBlendMode { Normal, Additive, Multiply, Screen };

// ReactivitySource is defined in visual_module.h (VisualModule::render()
// needs it too; defining it there avoids a visuals/ <-> compositor/
// include cycle).

struct Layer {
    VisualType visual_type;
    std::unique_ptr<VisualModule> visual;
    float opacity = 1.0f;
    LayerBlendMode blend_mode = LayerBlendMode::Normal;
    Color tint_color = WHITE;
    // nullopt = Global; otherwise a band index in [0, audio_config::kNumBands).
    std::optional<std::size_t> freq_band;
    ReactivitySource reactivity_source = ReactivitySource::GlobalBeat;
    bool enabled = true;
};

std::unique_ptr<VisualModule> make_visual(VisualType type);
