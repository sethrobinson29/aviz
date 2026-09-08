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

// Maps band energies to radial line segments, one per band, evenly spaced
// by angle starting at 12 o'clock and proceeding clockwise. Each band's
// length is band_energy[i] normalized against its own band_reference[i]
// (same normalization spectrum_math::to_bars uses for a bar's height),
// direct-multiplied by `sensitivity` (may exceed 1.0), then scaled by the
// full gap between inner_radius_fraction and max_radius_fraction -- 0%
// reaches exactly the inner radius (zero length), 100% reaches exactly the
// outer radius, with no independent height cap or minimum-visible floor.
// `inner_radius_fraction`/`max_radius_fraction` are fractions of
// min(width, height) / 2, both applied around the texture's center. Callers
// are expected to keep inner strictly below outer by at least
// kMinRadiusGapFraction (clamp_inner_radius_fraction/clamp_max_radius_fraction
// below enforce this at the point of adjustment); this function floors the
// gap at 0 defensively but doesn't otherwise validate it.
std::vector<RadialSegment> to_radial_bars(const float* band_energy, const float* band_reference,
                                           std::size_t band_count, int width, int height,
                                           float inner_radius_fraction, float max_radius_fraction,
                                           float sensitivity = 1.0f);

// Minimum gap enforced between inner_radius_fraction and max_radius_fraction
// so the bar-length range (to_radial_bars' gap) can never collapse to zero
// or invert. Initial guess, unconfirmed against real material; tunable.
inline constexpr float kMinRadiusGapFraction = 0.05f;

// Clamps `inner` into [0, outer - kMinRadiusGapFraction] (the upper bound is
// itself floored at 0 if `outer` is too small to leave room for the gap).
float clamp_inner_radius_fraction(float inner, float outer);

// Clamps `outer` into [inner + kMinRadiusGapFraction, 1] (the lower bound is
// itself capped at 1 if `inner` is close enough to 1 that adding the gap
// would exceed it).
float clamp_max_radius_fraction(float outer, float inner);

}  // namespace radial_math

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
    void render(const AudioFeatures& features, const LayerReactivity& reactivity) override;
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
    // Same per-band onset-color-pulse mechanism as SpectrumVisual (see
    // spectrum_math::update_onset_pulse/pulse_color) -- reused directly
    // rather than reimplemented, since this visual is spectrum-style
    // reactivity with different geometry, not a different reactivity model.
    std::array<float, audio_config::kTotalBandSlots> onset_pulse_{};
};
