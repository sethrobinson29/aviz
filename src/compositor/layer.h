#pragma once

#include <memory>

#include "visuals/visual_module.h"

// Visual types available to the compositor (spec §6, build-order subset —
// only the two visuals that exist through milestone 2).
enum class VisualType { Waveform, Spectrum };

// Compositor-stage blend modes (spec §5). Applied per layer when its
// offscreen texture is drawn onto the compositor's output texture.
// Named LayerBlendMode, not BlendMode, to avoid colliding with raylib's
// own global BlendMode enum (BLEND_ALPHA, BLEND_ADDITIVE, ...).
enum class LayerBlendMode { Normal, Additive, Multiply, Screen };

// One entry in the layer stack. Stack position (i.e. index in
// Compositor::layers()) is the layer's order_index (spec §5) — no separate
// field, since the vector's order already is the stack order.
struct Layer {
    VisualType visual_type;
    std::unique_ptr<VisualModule> visual;
    float opacity = 1.0f;
    LayerBlendMode blend_mode = LayerBlendMode::Normal;
};

std::unique_ptr<VisualModule> make_visual(VisualType type);
