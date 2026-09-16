#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "raylib.h"
#include "visuals/visual_module.h"

namespace lissajous_math {

inline constexpr float kAmplitudeCompressionExponent = 0.5f;

float compress_sample(float sample, float exponent);

std::vector<Vector2> to_points(const float* left, const float* right, std::size_t count,
                                int width, int height, bool idle, float amplitude_scale = 1.0f,
                                float compression_exponent = kAmplitudeCompressionExponent);

}

struct LissajousParams {
    Color line_color = WHITE;
    float line_thickness = 2.0f;
};

class LissajousVisual : public VisualModule {
public:
    explicit LissajousVisual(LissajousParams params = {});
    ~LissajousVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features, const LayerReactivity& reactivity,
                const std::optional<Palette>& palette_override) override;
    RenderTexture2D& render_texture() override { return texture_; }
    std::unique_ptr<VisualModule> clone() const override {
        return std::make_unique<LissajousVisual>(params_);
    }
    const LissajousParams& params() const { return params_; }
    LissajousParams& mutable_params() { return params_; }

private:
    LissajousParams params_;
    RenderTexture2D texture_{};
    int width_ = 0;
    int height_ = 0;
    float onset_pulse_ = 0.0f;
    std::optional<std::size_t> last_freq_band_;
};
