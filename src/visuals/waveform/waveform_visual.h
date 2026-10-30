#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "raylib.h"
#include "visuals/visual_module.h"

namespace waveform_math {

std::vector<Vector2> to_points(const float* samples, std::size_t count, int width, int height,
                                bool idle, float amplitude_scale = 1.0f);

float band_amplitude_scale(float energy, float reference);

float pulse_thickness(float base_thickness, float pulse);

float clamp_line_thickness(double thickness);

float next_onset_pulse(float current_pulse, bool band_changed, bool onset_active,
                        float pulse_target, float decay_per_frame);

struct ReactivityResult {
    float amplitude_scale = 1.0f;
    bool onset_active = false;
    float onset_pulse_target = 0.0f;
};

ReactivityResult resolve_reactivity(const AudioFeatures& features, const LayerReactivity& reactivity);

std::size_t safe_band_index(std::optional<std::size_t> freq_band);

}

struct WaveformParams {
    Color line_color = WHITE;
    float line_thickness = 3.0f;
};

class WaveformVisual : public VisualModule {
public:
    explicit WaveformVisual(WaveformParams params = {});
    ~WaveformVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features, const LayerReactivity& reactivity,
                const std::optional<Palette>& palette_override) override;
    RenderTexture2D& render_texture() override { return texture_; }
    std::unique_ptr<VisualModule> clone() const override {
        return std::make_unique<WaveformVisual>(params_);
    }
    const WaveformParams& params() const { return params_; }

private:
    WaveformParams params_;
    RenderTexture2D texture_{};
    int width_ = 0;
    int height_ = 0;
    float onset_pulse_ = 0.0f;
    std::optional<std::size_t> last_freq_band_;
};
