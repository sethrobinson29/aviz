#include "compositor/compositor.h"

#include "compositor/compositor_math.h"
#include "rlgl.h"

Compositor::~Compositor() {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(output_);
    }
}

void Compositor::resize(int width, int height) {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(output_);
    }
    output_ = LoadRenderTexture(width, height);
    width_ = width;
    height_ = height;

    for (Layer& layer : layers_) {
        layer.visual->resize(width, height);
    }
}

std::size_t Compositor::add_layer(VisualType type) {
    Layer layer{type, make_visual(type)};
    if (width_ > 0 && height_ > 0) {
        layer.visual->resize(width_, height_);
    }
    layers_.push_back(std::move(layer));
    return layers_.size() - 1;
}

void Compositor::remove_layer(std::size_t index) {
    if (index >= layers_.size()) {
        return;
    }
    layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(index));
}

void Compositor::move_layer(std::size_t from, std::size_t to) {
    compositor_math::move_element(layers_, from, to);
}

void Compositor::render(const AudioFeatures& features) {
    for (Layer& layer : layers_) {
        layer.visual->render(features);
    }

    BeginTextureMode(output_);
    ClearBackground(BLANK);

    for (Layer& layer : layers_) {
        const compositor_math::BlendFactors factors =
            compositor_math::to_blend_factors(layer.blend_mode);
        if (factors.raylib_mode == BLEND_CUSTOM) {
            rlSetBlendFactors(factors.src_factor, factors.dst_factor, factors.equation);
        }
        BeginBlendMode(factors.raylib_mode);

        RenderTexture2D& tex = layer.visual->render_texture();
        const float alpha = compositor_math::clamp_opacity(layer.opacity) * 255.0f;
        // Negate height: offscreen textures are Y-flipped vs. screen/output.
        DrawTextureRec(tex.texture,
                        Rectangle{0.0f, 0.0f, static_cast<float>(tex.texture.width),
                                  -static_cast<float>(tex.texture.height)},
                        Vector2{0.0f, 0.0f},
                        Color{255, 255, 255, static_cast<unsigned char>(alpha)});

        EndBlendMode();
    }

    EndTextureMode();
}
