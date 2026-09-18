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

std::uint64_t Compositor::allocate_layer_id() { return next_layer_id_++; }

std::optional<std::size_t> Compositor::add_layer(VisualType type) {
    if (layers_.size() >= kMaxLayers) {
        return std::nullopt;
    }
    Layer layer{type, make_visual(type)};
    layer.layer_id = allocate_layer_id();
    if (width_ > 0 && height_ > 0) {
        layer.visual->resize(width_, height_);
    }
    layers_.push_back(std::move(layer));
    return layers_.size() - 1;
}

std::optional<std::size_t> Compositor::duplicate_layer(std::size_t index) {
    if (index >= layers_.size() || layers_.size() >= kMaxLayers) {
        return std::nullopt;
    }
    const Layer& source = layers_[index];
    Layer copy{source.visual_type, source.visual->clone()};
    copy.opacity = source.opacity;
    copy.blend_mode = source.blend_mode;
    copy.tint_color = source.tint_color;
    copy.palette_override = source.palette_override;
    copy.freq_band = source.freq_band;
    copy.reactivity_source = source.reactivity_source;
    copy.sensitivity = source.sensitivity;
    copy.enabled = source.enabled;
    copy.layer_id = allocate_layer_id();
    if (width_ > 0 && height_ > 0) {
        copy.visual->resize(width_, height_);
    }
    layers_.push_back(std::move(copy));
    return layers_.size() - 1;
}

bool Compositor::replace_layers_if_ids_valid(std::vector<Layer> layers) {
    if (layers.size() > kMaxLayers) {
        return false;
    }
    if (compositor_math::has_invalid_layer_id(layers)) {
        return false;
    }
    if (compositor_math::has_duplicate_layer_ids(layers)) {
        return false;
    }
    if (width_ > 0 && height_ > 0) {
        for (Layer& layer : layers) {
            layer.visual->resize(width_, height_);
        }
    }
    next_layer_id_ = compositor_math::next_layer_id_after_load(layers);
    layers_ = std::move(layers);
    return true;
}

void Compositor::set_layer_visual_type(std::size_t index, VisualType type) {
    if (index >= layers_.size()) {
        return;
    }
    Layer& layer = layers_[index];
    layer.visual = make_visual(type);
    layer.visual_type = type;
    if (width_ > 0 && height_ > 0) {
        layer.visual->resize(width_, height_);
    }
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
        if (layer.enabled) {
            layer.visual->render(features,
                                  LayerReactivity{layer.freq_band, layer.reactivity_source,
                                                   compositor_math::clamp_sensitivity(layer.sensitivity)},
                                  layer.palette_override);
        }
    }

    BeginTextureMode(output_);
    ClearBackground(BLANK);

    for (Layer& layer : layers_) {
        if (!layer.enabled) {
            continue;
        }
        const compositor_math::BlendFactors factors =
            compositor_math::to_blend_factors(layer.blend_mode);
        if (factors.raylib_mode == BLEND_CUSTOM) {
            rlSetBlendFactors(factors.src_factor, factors.dst_factor, factors.equation);
        }
        BeginBlendMode(factors.raylib_mode);

        RenderTexture2D& tex = layer.visual->render_texture();
        const float alpha = compositor_math::clamp_opacity(layer.opacity) * 255.0f;
        const Color& tint = layer.tint_color;
        DrawTextureRec(tex.texture,
                        Rectangle{0.0f, 0.0f, static_cast<float>(tex.texture.width),
                                  -static_cast<float>(tex.texture.height)},
                        Vector2{0.0f, 0.0f},
                        Color{tint.r, tint.g, tint.b, static_cast<unsigned char>(alpha)});

        EndBlendMode();
    }

    EndTextureMode();
}
