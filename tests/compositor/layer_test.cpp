#include "compositor/layer.h"

#include <gtest/gtest.h>

#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/fractal/fractal_visual.h"
#include "visuals/lissajous/lissajous_visual.h"
#include "visuals/particles/particles_visual.h"
#include "visuals/radial/radial_visual.h"
#include "visuals/spectrum/spectrum_visual.h"
#include "visuals/waveform/waveform_visual.h"

TEST(MakeVisualTest, WaveformTypeConstructsWaveformVisual) {
    auto visual = make_visual(VisualType::Waveform);
    EXPECT_NE(dynamic_cast<WaveformVisual*>(visual.get()), nullptr);
    EXPECT_EQ(dynamic_cast<SpectrumVisual*>(visual.get()), nullptr);
}

TEST(MakeVisualTest, SpectrumTypeConstructsSpectrumVisual) {
    auto visual = make_visual(VisualType::Spectrum);
    EXPECT_NE(dynamic_cast<SpectrumVisual*>(visual.get()), nullptr);
    EXPECT_EQ(dynamic_cast<WaveformVisual*>(visual.get()), nullptr);
}

TEST(MakeVisualTest, RadialTypeConstructsRadialVisual) {
    auto visual = make_visual(VisualType::Radial);
    EXPECT_NE(dynamic_cast<RadialVisual*>(visual.get()), nullptr);
    EXPECT_EQ(dynamic_cast<SpectrumVisual*>(visual.get()), nullptr);
}

TEST(MakeVisualTest, ParticlesTypeConstructsParticlesVisual) {
    auto visual = make_visual(VisualType::Particles);
    EXPECT_NE(dynamic_cast<ParticlesVisual*>(visual.get()), nullptr);
    EXPECT_EQ(dynamic_cast<RadialVisual*>(visual.get()), nullptr);
}

TEST(MakeVisualTest, LissajousTypeConstructsLissajousVisual) {
    auto visual = make_visual(VisualType::Lissajous);
    EXPECT_NE(dynamic_cast<LissajousVisual*>(visual.get()), nullptr);
    EXPECT_EQ(dynamic_cast<WaveformVisual*>(visual.get()), nullptr);
}

TEST(MakeVisualTest, FlowFieldTypeConstructsFlowFieldVisual) {
    auto visual = make_visual(VisualType::FlowField);
    EXPECT_NE(dynamic_cast<FlowFieldVisual*>(visual.get()), nullptr);
    EXPECT_EQ(dynamic_cast<ParticlesVisual*>(visual.get()), nullptr);
}

TEST(MakeVisualTest, FractalTypeConstructsFractalVisual) {
    auto visual = make_visual(VisualType::Fractal);
    EXPECT_NE(dynamic_cast<FractalVisual*>(visual.get()), nullptr);
    EXPECT_EQ(dynamic_cast<WaveformVisual*>(visual.get()), nullptr);
}

TEST(VisualCloneTest, WaveformCloneIsIndependentInstanceOfSameType) {
    auto original = make_visual(VisualType::Waveform);
    auto clone = original->clone();

    EXPECT_NE(clone.get(), original.get());
    EXPECT_NE(dynamic_cast<WaveformVisual*>(clone.get()), nullptr);
}

TEST(VisualCloneTest, WaveformClonePreservesParams) {
    WaveformVisual original(WaveformParams{BLUE, 5.0f});
    auto clone = original.clone();

    auto* waveform_clone = dynamic_cast<WaveformVisual*>(clone.get());
    ASSERT_NE(waveform_clone, nullptr);
    const WaveformParams& params = waveform_clone->params();
    EXPECT_EQ(params.line_color.r, BLUE.r);
    EXPECT_EQ(params.line_color.g, BLUE.g);
    EXPECT_EQ(params.line_color.b, BLUE.b);
    EXPECT_FLOAT_EQ(params.line_thickness, 5.0f);
}

TEST(VisualCloneTest, SpectrumCloneIsIndependentInstanceOfSameType) {
    auto original = make_visual(VisualType::Spectrum);
    auto clone = original->clone();

    EXPECT_NE(clone.get(), original.get());
    EXPECT_NE(dynamic_cast<SpectrumVisual*>(clone.get()), nullptr);
}

TEST(VisualCloneTest, SpectrumClonePreservesParams) {
    SpectrumVisual original(SpectrumParams{RED, 0.5f, 0.9f});
    auto clone = original.clone();

    auto* spectrum_clone = dynamic_cast<SpectrumVisual*>(clone.get());
    ASSERT_NE(spectrum_clone, nullptr);
    const SpectrumParams& params = spectrum_clone->params();
    EXPECT_EQ(params.bar_color.r, RED.r);
    EXPECT_EQ(params.bar_color.g, RED.g);
    EXPECT_EQ(params.bar_color.b, RED.b);
    EXPECT_FLOAT_EQ(params.gap_ratio, 0.5f);
    EXPECT_FLOAT_EQ(params.max_height_fraction, 0.9f);
}

TEST(VisualCloneTest, RadialCloneIsIndependentInstanceOfSameType) {
    auto original = make_visual(VisualType::Radial);
    auto clone = original->clone();

    EXPECT_NE(clone.get(), original.get());
    EXPECT_NE(dynamic_cast<RadialVisual*>(clone.get()), nullptr);
}

TEST(VisualCloneTest, RadialClonePreservesParams) {
    RadialVisual original(RadialParams{GREEN, 4.0f, 0.2f, 0.8f});
    auto clone = original.clone();

    auto* radial_clone = dynamic_cast<RadialVisual*>(clone.get());
    ASSERT_NE(radial_clone, nullptr);
    const RadialParams& params = radial_clone->params();
    EXPECT_EQ(params.bar_color.r, GREEN.r);
    EXPECT_EQ(params.bar_color.g, GREEN.g);
    EXPECT_EQ(params.bar_color.b, GREEN.b);
    EXPECT_FLOAT_EQ(params.thickness, 4.0f);
    EXPECT_FLOAT_EQ(params.inner_radius_fraction, 0.2f);
    EXPECT_FLOAT_EQ(params.max_radius_fraction, 0.8f);
}

TEST(VisualCloneTest, ParticlesCloneIsIndependentInstanceOfSameType) {
    auto original = make_visual(VisualType::Particles);
    auto clone = original->clone();

    EXPECT_NE(clone.get(), original.get());
    EXPECT_NE(dynamic_cast<ParticlesVisual*>(clone.get()), nullptr);
}

TEST(VisualCloneTest, ParticlesClonePreservesParams) {
    ParticlesVisual original(ParticlesParams{ORANGE, 5.0f, 90, 30, 3.0f});
    auto clone = original.clone();

    auto* particles_clone = dynamic_cast<ParticlesVisual*>(clone.get());
    ASSERT_NE(particles_clone, nullptr);
    const ParticlesParams& params = particles_clone->params();
    EXPECT_EQ(params.particle_color.r, ORANGE.r);
    EXPECT_EQ(params.particle_color.g, ORANGE.g);
    EXPECT_EQ(params.particle_color.b, ORANGE.b);
    EXPECT_FLOAT_EQ(params.radius, 5.0f);
    EXPECT_EQ(params.lifetime_frames, 90);
    EXPECT_EQ(params.burst_count, 30);
    EXPECT_FLOAT_EQ(params.spawn_rate_per_frame, 3.0f);
}

TEST(VisualCloneTest, LissajousCloneIsIndependentInstanceOfSameType) {
    auto original = make_visual(VisualType::Lissajous);
    auto clone = original->clone();

    EXPECT_NE(clone.get(), original.get());
    EXPECT_NE(dynamic_cast<LissajousVisual*>(clone.get()), nullptr);
}

TEST(VisualCloneTest, LissajousClonePreservesParams) {
    LissajousVisual original(LissajousParams{PURPLE, 4.5f, 0.8f});
    auto clone = original.clone();

    auto* lissajous_clone = dynamic_cast<LissajousVisual*>(clone.get());
    ASSERT_NE(lissajous_clone, nullptr);
    const LissajousParams& params = lissajous_clone->params();
    EXPECT_EQ(params.line_color.r, PURPLE.r);
    EXPECT_EQ(params.line_color.g, PURPLE.g);
    EXPECT_EQ(params.line_color.b, PURPLE.b);
    EXPECT_FLOAT_EQ(params.line_thickness, 4.5f);
    EXPECT_FLOAT_EQ(params.compression_exponent, 0.8f);
}

TEST(VisualCloneTest, FlowFieldCloneIsIndependentInstanceOfSameType) {
    auto original = make_visual(VisualType::FlowField);
    auto clone = original->clone();

    EXPECT_NE(clone.get(), original.get());
    EXPECT_NE(dynamic_cast<FlowFieldVisual*>(clone.get()), nullptr);
}

TEST(VisualCloneTest, FlowFieldClonePreservesParams) {
    FlowFieldParams params;
    params.particle_color = SKYBLUE;
    params.radius = 1.5f;
    params.lifetime_frames = 500;
    params.burst_count = 10;
    params.spawn_rate_per_frame = 0.4f;
    params.ripple_on_onset_enabled = false;
    params.burst_on_onset_enabled = false;
    FlowFieldVisual original(params);
    auto clone = original.clone();

    auto* flow_field_clone = dynamic_cast<FlowFieldVisual*>(clone.get());
    ASSERT_NE(flow_field_clone, nullptr);
    const FlowFieldParams& cloned_params = flow_field_clone->params();
    EXPECT_EQ(cloned_params.particle_color.r, SKYBLUE.r);
    EXPECT_EQ(cloned_params.particle_color.g, SKYBLUE.g);
    EXPECT_EQ(cloned_params.particle_color.b, SKYBLUE.b);
    EXPECT_FLOAT_EQ(cloned_params.radius, 1.5f);
    EXPECT_EQ(cloned_params.lifetime_frames, 500);
    EXPECT_EQ(cloned_params.burst_count, 10);
    EXPECT_FLOAT_EQ(cloned_params.spawn_rate_per_frame, 0.4f);
    EXPECT_FALSE(cloned_params.ripple_on_onset_enabled);
    EXPECT_FALSE(cloned_params.burst_on_onset_enabled);
}

TEST(VisualCloneTest, FractalCloneIsIndependentInstanceOfSameType) {
    auto original = make_visual(VisualType::Fractal);
    auto clone = original->clone();

    EXPECT_NE(clone.get(), original.get());
    EXPECT_NE(dynamic_cast<FractalVisual*>(clone.get()), nullptr);
}

TEST(VisualCloneTest, FractalClonePreservesParams) {
    FractalParams params;
    params.base_zoom = 1.5f;
    params.zoom_amplitude_scale = 0.3f;
    params.julia_base_radius = 0.75f;
    params.julia_radius_amplitude_scale = 0.1f;
    params.julia_angular_speed_radians_per_sec = 0.25f;
    params.julia_orbit_center_x = 0.2f;
    params.julia_orbit_center_y = -0.1f;
    params.julia_orbit_clockwise = false;
    params.color_cycle_speed_hz = 0.08f;
    params.amplitude_smoothing_tau_ms = 1500.0f;
    params.boundary_margin_px = 25.0f;
    FractalVisual original(params);
    auto clone = original.clone();

    auto* fractal_clone = dynamic_cast<FractalVisual*>(clone.get());
    ASSERT_NE(fractal_clone, nullptr);
    const FractalParams& cloned = fractal_clone->params();
    EXPECT_FLOAT_EQ(cloned.base_zoom, 1.5f);
    EXPECT_FLOAT_EQ(cloned.zoom_amplitude_scale, 0.3f);
    EXPECT_FLOAT_EQ(cloned.julia_base_radius, 0.75f);
    EXPECT_FLOAT_EQ(cloned.julia_radius_amplitude_scale, 0.1f);
    EXPECT_FLOAT_EQ(cloned.julia_angular_speed_radians_per_sec, 0.25f);
    EXPECT_FLOAT_EQ(cloned.julia_orbit_center_x, 0.2f);
    EXPECT_FLOAT_EQ(cloned.julia_orbit_center_y, -0.1f);
    EXPECT_FALSE(cloned.julia_orbit_clockwise);
    EXPECT_FLOAT_EQ(cloned.color_cycle_speed_hz, 0.08f);
    EXPECT_FLOAT_EQ(cloned.amplitude_smoothing_tau_ms, 1500.0f);
    EXPECT_FLOAT_EQ(cloned.boundary_margin_px, 25.0f);
}
