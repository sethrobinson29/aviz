#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "audio/audio_config.h"
#include "raylib.h"
#include "visuals/visual_module.h"

namespace spectrum_math {

// Maps band energies to bar rectangles, one bar per band, left to right
// low-to-high frequency. Each band's height is band_energy[i] normalized
// against its own band_reference[i] then direct-multiplied by `sensitivity`
// (may exceed 1.0 for sensitivity > 1); `band_reference[i] <= 0` yields a
// zero-height bar regardless of sensitivity. `max_height_fraction` caps a
// fully-normalized (sensitivity == 1) bar short of the window edge. `idle`
// returns zero-height bars, ignoring both arrays and sensitivity.
std::vector<Rectangle> to_bars(const float* band_energy, const float* band_reference,
                                std::size_t band_count, int width, int height, float gap_ratio,
                                float max_height_fraction, bool idle, float sensitivity = 1.0f);

// Indices into a to_bars() result to actually draw: {*freq_band} alone when
// set (a band-scoped layer draws only its own bar), or every index
// [0, bar_count) in order for Global. An out-of-range freq_band (>=
// bar_count) yields no indices rather than an invalid one.
std::vector<std::size_t> bar_indices_to_draw(std::optional<std::size_t> freq_band,
                                              std::size_t bar_count);

// Next value for a band's onset color pulse: snaps to `sensitivity`
// (pre-clamp) when `onset` is true, else `current_pulse` decayed by
// visual_config::kSpectrumOnsetPulseDecayPerFrame.
float update_onset_pulse(float current_pulse, bool onset, float sensitivity);

// Lerps each of base's RGB channels between a dim rest floor (pulse == 0,
// see visual_config::kSpectrumOnsetPulseRestBrightnessFraction) and base
// itself, unchanged (pulse == 1), clamping `pulse` to [0, 1] first. Alpha
// is always base.a.
Color pulse_color(Color base, float pulse);

}  // namespace spectrum_math

struct SpectrumParams {
    Color bar_color = WHITE;
    float gap_ratio = 0.2f;            // gap width as a fraction of bar width
    float max_height_fraction = 0.8f;
};

class SpectrumVisual : public VisualModule {
public:
    explicit SpectrumVisual(SpectrumParams params = {});
    ~SpectrumVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features, const LayerReactivity& reactivity) override;
    RenderTexture2D& render_texture() override { return texture_; }
    std::unique_ptr<VisualModule> clone() const override {
        return std::make_unique<SpectrumVisual>(params_);
    }
    const SpectrumParams& params() const { return params_; }

private:
    SpectrumParams params_;
    RenderTexture2D texture_{};
    int width_ = 0;
    int height_ = 0;
    // One decaying onset-color pulse per band + global (kTotalBandSlots),
    // updated every render() call from each band's own onset flag. Not
    // part of params_/clone() -- a clone starts with no pulses in flight,
    // same as WaveformVisual's onset_pulse_.
    std::array<float, audio_config::kTotalBandSlots> onset_pulse_{};
};
