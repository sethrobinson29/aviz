#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "audio/audio_features.h"
#include "compositor/layer.h"
#include "raylib.h"

class Compositor {
public:
    static constexpr std::size_t kMaxLayers = 16;

    ~Compositor();

    void resize(int width, int height);

    std::optional<std::size_t> add_layer(VisualType type);
    std::optional<std::size_t> duplicate_layer(std::size_t index);
    void set_layer_visual_type(std::size_t index, VisualType type);
    void remove_layer(std::size_t index);
    void move_layer(std::size_t from, std::size_t to);

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
