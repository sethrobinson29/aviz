#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "raylib.h"
#include "visuals/visual_module.h"

namespace waveform_math {

// Maps a mono waveform sample window to screen-space points for a line
// strip. `amplitude_scale` multiplies every sample before it's mapped to a
// Y offset (1.0 = unmodified). When `idle` is true, returns a flat
// mid-height line instead, ignoring amplitude_scale.
std::vector<Vector2> to_points(const float* samples, std::size_t count, int width, int height,
                                bool idle, float amplitude_scale = 1.0f);

// energy/reference normalized to [0, 1], clamped; reference <= 0 yields 0.
// Same normalization spectrum_math::to_bars uses for a single band.
float band_amplitude_scale(float energy, float reference);

// base_thickness scaled up by `pulse` (clamped to [0, 1]) via
// visual_config::kOnsetPulseThicknessBoost; pulse == 0 returns base_thickness
// unchanged.
float pulse_thickness(float base_thickness, float pulse);

}  // namespace waveform_math

struct WaveformParams {
    Color line_color = WHITE;
    float line_thickness = 3.0f;
};

class WaveformVisual : public VisualModule {
public:
    explicit WaveformVisual(WaveformParams params = {});
    ~WaveformVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features, const LayerReactivity& reactivity) override;
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
    // Snaps to 1.0 on a followed onset, decays toward 0 each rendered
    // frame otherwise. Not part of params_/clone() -- a clone starts with
    // no pulse in flight, same as a freshly constructed instance.
    float onset_pulse_ = 0.0f;
};
