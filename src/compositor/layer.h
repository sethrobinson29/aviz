#pragma once

#include <memory>

#include "visuals/visual_module.h"

enum class VisualType { Waveform, Spectrum };

// Blend modes applied per layer when its offscreen texture is drawn onto
// the compositor's output texture.
enum class LayerBlendMode { Normal, Additive, Multiply, Screen };

struct Layer {
    VisualType visual_type;
    std::unique_ptr<VisualModule> visual;
    float opacity = 1.0f;
    LayerBlendMode blend_mode = LayerBlendMode::Normal;
};

std::unique_ptr<VisualModule> make_visual(VisualType type);
