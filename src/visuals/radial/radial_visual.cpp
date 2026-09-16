#include "visuals/radial/radial_visual.h"

#include <algorithm>
#include <cmath>

#include "audio/audio_config.h"
#include "visuals/spectrum/spectrum_visual.h"

namespace radial_math {

std::vector<RadialSegment> to_radial_bars(const float* band_energy, const float* band_reference,
                                           std::size_t band_count, int width, int height,
                                           float inner_radius_fraction, float max_radius_fraction,
                                           float sensitivity) {
    std::vector<RadialSegment> segments;
    if (band_count == 0) {
        return segments;
    }
    segments.reserve(band_count);

    const Vector2 center{static_cast<float>(width) * 0.5f, static_cast<float>(height) * 0.5f};
    const float unit_radius = std::min(width, height) * 0.5f;
    const float inner_radius = unit_radius * inner_radius_fraction;
    const float max_outer_radius = unit_radius * max_radius_fraction;
    const float max_length = std::max(max_outer_radius - inner_radius, 0.0f);
    const float angle_step = 2.0f * PI / static_cast<float>(band_count);

    for (std::size_t i = 0; i < band_count; ++i) {
        float normalized = 0.0f;
        if (band_reference[i] > 0.0f) {
            normalized = std::clamp(band_energy[i] / band_reference[i], 0.0f, 1.0f) * sensitivity;
        }
        const float length = normalized * max_length;
        const float angle = -PI * 0.5f + static_cast<float>(i) * angle_step;
        const float cos_a = std::cos(angle);
        const float sin_a = std::sin(angle);
        segments.push_back(RadialSegment{
            Vector2{center.x + inner_radius * cos_a, center.y + inner_radius * sin_a},
            Vector2{center.x + (inner_radius + length) * cos_a,
                    center.y + (inner_radius + length) * sin_a}});
    }
    return segments;
}

float clamp_inner_radius_fraction(float inner, float outer) {
    const float upper = std::max(outer - kMinRadiusGapFraction, 0.0f);
    return std::clamp(inner, 0.0f, upper);
}

float clamp_max_radius_fraction(float outer, float inner) {
    const float lower = std::min(inner + kMinRadiusGapFraction, 1.0f);
    return std::clamp(outer, lower, 1.0f);
}

}

RadialVisual::RadialVisual(RadialParams params) : params_(params) {}

RadialVisual::~RadialVisual() {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
}

void RadialVisual::resize(int width, int height) {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
    texture_ = LoadRenderTexture(width, height);
    width_ = width;
    height_ = height;
}

void RadialVisual::render(const AudioFeatures& features, const LayerReactivity& reactivity,
                           const std::optional<Palette>& palette_override) {
    const bool onset_mode = reactivity.source == ReactivitySource::BandOnset ||
                             reactivity.source == ReactivitySource::GlobalBeat;

    for (std::size_t i = 0; i < audio_config::kTotalBandSlots; ++i) {
        onset_pulse_[i] = spectrum_math::update_onset_pulse(onset_pulse_[i], features.band_onset[i],
                                                              reactivity.sensitivity);
    }

    const std::vector<radial_math::RadialSegment> segments = radial_math::to_radial_bars(
        features.band_energy.data(), features.band_agc_reference.data(), audio_config::kNumBands,
        width_, height_, params_.inner_radius_fraction, params_.max_radius_fraction,
        reactivity.sensitivity);

    BeginTextureMode(texture_);
    ClearBackground(BLANK);
    for (std::size_t i : spectrum_math::bar_indices_to_draw(reactivity.freq_band, segments.size())) {
        Color draw_color =
            spectrum_math::bar_base_color(params_.bar_color, palette_override, i, segments.size());
        if (onset_mode) {
            draw_color = spectrum_math::pulse_color(draw_color, onset_pulse_[i]);
        }
        DrawLineEx(segments[i].inner, segments[i].outer, params_.thickness, draw_color);
    }
    EndTextureMode();
}
