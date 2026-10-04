#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "audio/audio_config.h"
#include "raylib.h"
#include "visuals/visual_module.h"

namespace radial_math {

struct RadialSegment {
    Vector2 inner;
    Vector2 outer;
};

std::vector<RadialSegment> to_radial_bars(const float* band_energy, const float* band_reference,
                                           std::size_t band_count, int width, int height,
                                           float inner_radius_fraction, float max_radius_fraction,
                                           float sensitivity = 1.0f);

inline constexpr float kMinRadiusGapFraction = 0.05f;

float clamp_inner_radius_fraction(double inner, float outer);

float clamp_max_radius_fraction(double outer, float inner);

}

struct RadialParams {
    Color bar_color = WHITE;
    float thickness = 3.0f;
    float inner_radius_fraction = 0.15f;
    float max_radius_fraction = 0.9f;
};

class RadialVisual : public VisualModule {
public:
    explicit RadialVisual(RadialParams params = {});
    ~RadialVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features, const LayerReactivity& reactivity,
                const std::optional<Palette>& palette_override) override;
    RenderTexture2D& render_texture() override { return texture_; }
    std::unique_ptr<VisualModule> clone() const override {
        return std::make_unique<RadialVisual>(params_);
    }
    const RadialParams& params() const { return params_; }
    RadialParams& mutable_params() { return params_; }

private:
    RadialParams params_;
    RenderTexture2D texture_{};
    int width_ = 0;
    int height_ = 0;
    std::array<float, audio_config::kTotalBandSlots> onset_pulse_{};
};
