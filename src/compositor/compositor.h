#pragma once

#include <vector>

#include "audio/audio_features.h"
#include "compositor/layer.h"
#include "raylib.h"

// Stacks layers into a single composited output texture. Each layer
// renders to its own offscreen texture (VisualModule owns that); the
// compositor only draws those textures on top of each other into
// `output_`, applying per-layer opacity and blend mode.
class Compositor {
public:
    ~Compositor();

    // Requires a live raylib window/GL context. Resizes the output texture
    // and every existing layer's texture.
    void resize(int width, int height);

    // Appends a new layer of the given type to the top of the stack.
    // Returns its index. Requires resize() to have been called first if
    // any layer is to render at nonzero size.
    std::size_t add_layer(VisualType type);
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
