#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "audio/audio_features.h"
#include "compositor/layer.h"
#include "raylib.h"

// Stacks layers into a single composited output texture. Each layer
// renders to its own offscreen texture (VisualModule owns that); the
// compositor only draws those textures on top of each other into
// `output_`, applying per-layer opacity, blend mode, and tint.
class Compositor {
public:
    // One per band + one for Global, matching the 16-band + global layout
    // in spec §4.
    static constexpr std::size_t kMaxLayers = 16;

    ~Compositor();

    // Requires a live raylib window/GL context. Resizes the output texture
    // and every existing layer's texture.
    void resize(int width, int height);

    // Appends a new layer of the given type to the top of the stack.
    // Returns its index, or nullopt (no-op) if the stack is already at
    // kMaxLayers. Requires resize() to have been called first if any layer
    // is to render at nonzero size.
    std::optional<std::size_t> add_layer(VisualType type);
    // Clones layers()[index]'s visual and appends a copy of the layer
    // (same params, same visual_type) to the top of the stack. Returns the
    // new layer's index, or nullopt if `index` is out of range or the
    // stack is already at kMaxLayers.
    std::optional<std::size_t> duplicate_layer(std::size_t index);
    // Replaces layers()[index]'s visual with a freshly constructed one of
    // `type`, resized to match the compositor's current dimensions. Every
    // other field on the layer (opacity, blend_mode, tint_color,
    // freq_band, reactivity_source, enabled) is left untouched. No-op if
    // `index` is out of range.
    void set_layer_visual_type(std::size_t index, VisualType type);
    void remove_layer(std::size_t index);
    // Shifts layers between `from` and `to`, rather than swapping them.
    void move_layer(std::size_t from, std::size_t to);

    // Renders every layer, then composites them into the output texture in
    // stack order (index 0 first / bottom).
    void render(const AudioFeatures& features);
    RenderTexture2D& output() { return output_; }

    std::vector<Layer>& layers() { return layers_; }
    const std::vector<Layer>& layers() const { return layers_; }

private:
    std::vector<Layer> layers_;
    RenderTexture2D output_{};
    int width_ = 0;
    int height_ = 0;
};
