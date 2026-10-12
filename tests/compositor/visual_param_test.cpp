#include "compositor/visual_param.h"

#include <limits>
#include <memory>

#include <gtest/gtest.h>

#include "compositor/compositor.h"
#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/fractal/fractal_visual.h"
#include "visuals/lissajous/lissajous_visual.h"
#include "visuals/particles/particles_visual.h"
#include "visuals/radial/radial_visual.h"
#include "visuals/visual_config.h"
#include "visuals/waveform/waveform_visual.h"

TEST(VisualParamSetTest, SetsParticlesSpawnRatePerFrame) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "spawn_rate_per_frame", 5.0f));

    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor.layers()[0].visual);
    EXPECT_FLOAT_EQ(particles.params().spawn_rate_per_frame, 5.0f);
}

TEST(VisualParamSetTest, ClampsParticlesSpawnRatePerFrame) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);

    visual_param::set(compositor.layers()[0], "spawn_rate_per_frame", -5.0f);

    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor.layers()[0].visual);
    EXPECT_FLOAT_EQ(particles.params().spawn_rate_per_frame, visual_config::kParticlesSpawnRatePerFrameMin);
}

TEST(VisualParamSetTest, SetsAndClampsParticlesLifetimeFrames) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "lifetime_frames", 300.0f));
    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor.layers()[0].visual);
    EXPECT_EQ(particles.params().lifetime_frames, 300);

    visual_param::set(compositor.layers()[0], "lifetime_frames", 999999.0f);
    EXPECT_EQ(particles.params().lifetime_frames, visual_config::kParticlesLifetimeFramesMax);
}

TEST(VisualParamSetTest, UnknownParamNameOnParticlesReturnsFalseAndLeavesParamsUnchanged) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);
    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor.layers()[0].visual);
    float original_rate = particles.params().spawn_rate_per_frame;

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "not_a_real_param", 5.0f));
    EXPECT_FLOAT_EQ(particles.params().spawn_rate_per_frame, original_rate);
}

TEST(VisualParamSetTest, UnsupportedVisualTypeReturnsFalse) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "spawn_rate_per_frame", 5.0f));
}

TEST(VisualParamSetTest, BoolValueSentToParticlesFloatParamIsRejectedAndLeavesParamUnchanged) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);
    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor.layers()[0].visual);
    float original_rate = particles.params().spawn_rate_per_frame;

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "spawn_rate_per_frame", true));
    EXPECT_FLOAT_EQ(particles.params().spawn_rate_per_frame, original_rate);
}

TEST(VisualParamSetTest, SetsFlowFieldSpawnRatePerFrame) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "spawn_rate_per_frame", 2.5f));

    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);
    EXPECT_FLOAT_EQ(flow.params().spawn_rate_per_frame, 2.5f);
}

TEST(VisualParamSetTest, ClampsFlowFieldSpawnRateToItsOwnRangeNotParticlesRange) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);

    visual_param::set(compositor.layers()[0], "spawn_rate_per_frame", 20.0f);
    EXPECT_FLOAT_EQ(flow.params().spawn_rate_per_frame, visual_config::kFlowFieldSpawnRatePerFrameMax);

    visual_param::set(compositor.layers()[0], "spawn_rate_per_frame", -1.0f);
    EXPECT_FLOAT_EQ(flow.params().spawn_rate_per_frame, visual_config::kFlowFieldSpawnRatePerFrameMin);
}

TEST(VisualParamSetTest, SetsAndClampsFlowFieldLifetimeFrames) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "lifetime_frames", 250.0f));
    EXPECT_EQ(flow.params().lifetime_frames, 250);

    visual_param::set(compositor.layers()[0], "lifetime_frames", 999999.0f);
    EXPECT_EQ(flow.params().lifetime_frames, visual_config::kParticlesLifetimeFramesMax);

    visual_param::set(compositor.layers()[0], "lifetime_frames", -5.0f);
    EXPECT_EQ(flow.params().lifetime_frames, visual_config::kParticlesLifetimeFramesMin);
}

TEST(VisualParamSetTest, SetsFlowFieldRippleAndBurstTogglesIndependently) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);
    ASSERT_TRUE(flow.params().ripple_on_onset_enabled);
    ASSERT_TRUE(flow.params().burst_on_onset_enabled);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "ripple_on_onset_enabled", false));
    EXPECT_FALSE(flow.params().ripple_on_onset_enabled);
    EXPECT_TRUE(flow.params().burst_on_onset_enabled);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "burst_on_onset_enabled", false));
    EXPECT_FALSE(flow.params().burst_on_onset_enabled);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "ripple_on_onset_enabled", true));
    EXPECT_TRUE(flow.params().ripple_on_onset_enabled);
    EXPECT_FALSE(flow.params().burst_on_onset_enabled);
}

TEST(VisualParamSetTest, FloatValueSentToFlowFieldBoolParamIsRejectedNotCoerced) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "ripple_on_onset_enabled", 0.0f));
    EXPECT_TRUE(flow.params().ripple_on_onset_enabled);

    visual_param::set(compositor.layers()[0], "burst_on_onset_enabled", false);
    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "burst_on_onset_enabled", 1.0f));
    EXPECT_FALSE(flow.params().burst_on_onset_enabled);
}

TEST(VisualParamSetTest, BoolValueSentToFlowFieldFloatParamIsRejectedNotCoerced) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);
    float original_rate = flow.params().spawn_rate_per_frame;
    int original_lifetime = flow.params().lifetime_frames;

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "spawn_rate_per_frame", true));
    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "lifetime_frames", true));
    EXPECT_FLOAT_EQ(flow.params().spawn_rate_per_frame, original_rate);
    EXPECT_EQ(flow.params().lifetime_frames, original_lifetime);
}

TEST(VisualParamSetTest, UnknownParamNameOnFlowFieldReturnsFalseForEitherValueType) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "not_a_real_param", 1.0f));
    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "not_a_real_param", true));
}

TEST(VisualParamSetTest, ParticlesOnlyParamNameIsRejectedOnFlowField) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "inner_radius_fraction", 0.5f));
}

TEST(VisualParamSetTest, SetsAndClampsFlowFieldTurbulenceFrequency) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "turbulence_frequency", 0.03f));
    EXPECT_FLOAT_EQ(flow.params().turbulence_frequency, 0.03f);

    visual_param::set(compositor.layers()[0], "turbulence_frequency", 99.0f);
    EXPECT_FLOAT_EQ(flow.params().turbulence_frequency, visual_config::kFlowFieldTurbulenceFrequencyMax);
}

TEST(VisualParamSetTest, SetsAndClampsFlowFieldEvolutionPerFrame) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "evolution_per_frame", 0.04f));
    EXPECT_FLOAT_EQ(flow.params().evolution_per_frame, 0.04f);

    visual_param::set(compositor.layers()[0], "evolution_per_frame", -3.0f);
    EXPECT_FLOAT_EQ(flow.params().evolution_per_frame, visual_config::kFlowFieldEvolutionPerFrameMin);
}

TEST(VisualParamSetTest, SetsAndClampsFlowFieldGridCols) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "grid_cols", 16.0f));
    EXPECT_EQ(flow.params().grid_cols, 16);

    visual_param::set(compositor.layers()[0], "grid_cols", 5000.0f);
    EXPECT_EQ(flow.params().grid_cols, visual_config::kFlowFieldGridColsMax);

    visual_param::set(compositor.layers()[0], "grid_cols", 0.0f);
    EXPECT_EQ(flow.params().grid_cols, visual_config::kFlowFieldGridColsMin);
}

TEST(VisualParamSetTest, BoolValueSentToFlowFieldFieldShapeParamsIsRejected) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "turbulence_frequency", true));
    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "evolution_per_frame", true));
    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "grid_cols", true));
    EXPECT_FLOAT_EQ(flow.params().turbulence_frequency, visual_config::kFlowFieldBaseTurbulenceFrequency);
    EXPECT_FLOAT_EQ(flow.params().evolution_per_frame, visual_config::kFlowFieldBaseEvolutionPerFrame);
    EXPECT_EQ(flow.params().grid_cols, visual_config::kFlowFieldGridCols);
}

TEST(VisualParamSetTest, ExtremeFloatLifetimeFramesClampsForParticlesAndFlowField) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);
    compositor.add_layer(VisualType::FlowField);
    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor.layers()[0].visual);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[1].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "lifetime_frames", 1e30f));
    EXPECT_TRUE(visual_param::set(compositor.layers()[1], "lifetime_frames", 1e30f));
    EXPECT_EQ(particles.params().lifetime_frames, visual_config::kParticlesLifetimeFramesMax);
    EXPECT_EQ(flow.params().lifetime_frames, visual_config::kParticlesLifetimeFramesMax);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "lifetime_frames", -1e30f));
    EXPECT_TRUE(visual_param::set(compositor.layers()[1], "lifetime_frames", -1e30f));
    EXPECT_EQ(particles.params().lifetime_frames, visual_config::kParticlesLifetimeFramesMin);
    EXPECT_EQ(flow.params().lifetime_frames, visual_config::kParticlesLifetimeFramesMin);
}

TEST(VisualParamSetTest, ExtremeFloatGridColsClampsForFlowField) {
    Compositor compositor;
    compositor.add_layer(VisualType::FlowField);
    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "grid_cols", 1e30f));
    EXPECT_EQ(flow.params().grid_cols, visual_config::kFlowFieldGridColsMax);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "grid_cols", -1e30f));
    EXPECT_EQ(flow.params().grid_cols, visual_config::kFlowFieldGridColsMin);
}

TEST(VisualParamSetTest, SetsLissajousCompressionExponent) {
    Compositor compositor;
    compositor.add_layer(VisualType::Lissajous);
    auto& lissajous = dynamic_cast<LissajousVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "compression_exponent", 0.8f));
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, 0.8f);
}

TEST(VisualParamSetTest, ClampsLissajousCompressionExponentIncludingExtremeAndNonFiniteValues) {
    Compositor compositor;
    compositor.add_layer(VisualType::Lissajous);
    auto& lissajous = dynamic_cast<LissajousVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "compression_exponent", 0.0f));
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, visual_config::kLissajousCompressionExponentMin);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "compression_exponent", 1e30f));
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, visual_config::kLissajousCompressionExponentMax);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "compression_exponent", -1e30f));
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, visual_config::kLissajousCompressionExponentMin);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "compression_exponent",
                                  std::numeric_limits<float>::infinity()));
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, visual_config::kLissajousCompressionExponentMax);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "compression_exponent",
                                  std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, visual_config::kLissajousCompressionExponentMin);
}

TEST(VisualParamSetTest, BoolValueSentToLissajousCompressionExponentIsRejectedNotCoerced) {
    Compositor compositor;
    compositor.add_layer(VisualType::Lissajous);
    auto& lissajous = dynamic_cast<LissajousVisual&>(*compositor.layers()[0].visual);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "compression_exponent", true));
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, lissajous_math::kAmplitudeCompressionExponent);
}

TEST(VisualParamSetTest, UnknownOrOtherModulesParamNameIsRejectedOnLissajous) {
    Compositor compositor;
    compositor.add_layer(VisualType::Lissajous);
    auto& lissajous = dynamic_cast<LissajousVisual&>(*compositor.layers()[0].visual);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "not_a_real_param", 0.8f));
    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "spawn_rate_per_frame", 0.8f));
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, lissajous_math::kAmplitudeCompressionExponent);
}

TEST(VisualParamSetTest, LissajousParamNameIsRejectedOnOtherVisualTypes) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);
    compositor.add_layer(VisualType::FlowField);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "compression_exponent", 0.8f));
    EXPECT_FALSE(visual_param::set(compositor.layers()[1], "compression_exponent", 0.8f));
}

TEST(VisualParamSetTest, SetsRadialInnerAndOuterRadiusFractionsIndependently) {
    Compositor compositor;
    compositor.add_layer(VisualType::Radial);
    auto& radial = dynamic_cast<RadialVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "inner_radius_fraction", 0.4f));
    EXPECT_FLOAT_EQ(radial.params().inner_radius_fraction, 0.4f);
    EXPECT_FLOAT_EQ(radial.params().max_radius_fraction, 0.9f);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "max_radius_fraction", 0.7f));
    EXPECT_FLOAT_EQ(radial.params().inner_radius_fraction, 0.4f);
    EXPECT_FLOAT_EQ(radial.params().max_radius_fraction, 0.7f);
}

TEST(VisualParamSetTest, RadialInnerRadiusStopsBelowOuterAndLeavesOuterUnmoved) {
    Compositor compositor;
    compositor.add_layer(VisualType::Radial);
    auto& radial = dynamic_cast<RadialVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "inner_radius_fraction", 0.95f));

    EXPECT_NEAR(radial.params().inner_radius_fraction, 0.9f - radial_math::kMinRadiusGapFraction, 1e-6f);
    EXPECT_FLOAT_EQ(radial.params().max_radius_fraction, 0.9f);
}

TEST(VisualParamSetTest, RadialOuterRadiusStopsAboveInnerAndLeavesInnerUnmoved) {
    Compositor compositor;
    compositor.add_layer(VisualType::Radial);
    auto& radial = dynamic_cast<RadialVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "max_radius_fraction", 0.0f));

    EXPECT_NEAR(radial.params().max_radius_fraction, 0.15f + radial_math::kMinRadiusGapFraction, 1e-6f);
    EXPECT_FLOAT_EQ(radial.params().inner_radius_fraction, 0.15f);
}

TEST(VisualParamSetTest, RadialInnerRadiusCanGoFurtherOnlyAfterOuterIsRaised) {
    Compositor compositor;
    compositor.add_layer(VisualType::Radial);
    auto& radial = dynamic_cast<RadialVisual&>(*compositor.layers()[0].visual);
    visual_param::set(compositor.layers()[0], "max_radius_fraction", 0.5f);

    visual_param::set(compositor.layers()[0], "inner_radius_fraction", 0.8f);
    EXPECT_NEAR(radial.params().inner_radius_fraction, 0.5f - radial_math::kMinRadiusGapFraction, 1e-6f);

    visual_param::set(compositor.layers()[0], "max_radius_fraction", 1.0f);
    visual_param::set(compositor.layers()[0], "inner_radius_fraction", 0.8f);
    EXPECT_FLOAT_EQ(radial.params().inner_radius_fraction, 0.8f);
}

TEST(VisualParamSetTest, RadialRadiusFractionsClampExtremeAndNonFiniteValuesAndKeepTheGap) {
    Compositor compositor;
    compositor.add_layer(VisualType::Radial);
    auto& radial = dynamic_cast<RadialVisual&>(*compositor.layers()[0].visual);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    for (float extreme : {1e30f, infinity}) {
        EXPECT_TRUE(visual_param::set(compositor.layers()[0], "max_radius_fraction", extreme));
        EXPECT_FLOAT_EQ(radial.params().max_radius_fraction, 1.0f);
        EXPECT_TRUE(visual_param::set(compositor.layers()[0], "inner_radius_fraction", extreme));
        EXPECT_FLOAT_EQ(radial.params().inner_radius_fraction, 1.0f - radial_math::kMinRadiusGapFraction);
    }

    for (float extreme : {-1e30f, -infinity, nan}) {
        EXPECT_TRUE(visual_param::set(compositor.layers()[0], "inner_radius_fraction", extreme));
        EXPECT_FLOAT_EQ(radial.params().inner_radius_fraction, 0.0f);
        EXPECT_TRUE(visual_param::set(compositor.layers()[0], "max_radius_fraction", extreme));
        EXPECT_FLOAT_EQ(radial.params().max_radius_fraction, radial_math::kMinRadiusGapFraction);
    }
}

TEST(VisualParamSetTest, BoolValueSentToRadialRadiusFractionsIsRejectedNotCoerced) {
    Compositor compositor;
    compositor.add_layer(VisualType::Radial);
    auto& radial = dynamic_cast<RadialVisual&>(*compositor.layers()[0].visual);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "inner_radius_fraction", true));
    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "max_radius_fraction", false));
    EXPECT_FLOAT_EQ(radial.params().inner_radius_fraction, 0.15f);
    EXPECT_FLOAT_EQ(radial.params().max_radius_fraction, 0.9f);
}

TEST(VisualParamSetTest, UnknownParamNameIsRejectedOnRadial) {
    Compositor compositor;
    compositor.add_layer(VisualType::Radial);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "not_a_real_param", 0.5f));
    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "compression_exponent", 0.5f));
}

TEST(VisualParamSetTest, SetsParticlesInnerRadiusFraction) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);
    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor.layers()[0].visual);

    EXPECT_TRUE(visual_param::set(compositor.layers()[0], "inner_radius_fraction", 0.35f));
    EXPECT_FLOAT_EQ(particles.params().inner_radius_fraction, 0.35f);
}

TEST(VisualParamSetTest, ClampsParticlesInnerRadiusFractionIncludingExtremeAndNonFiniteValues) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);
    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor.layers()[0].visual);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    for (float high : {1.5f, 1e30f, infinity}) {
        EXPECT_TRUE(visual_param::set(compositor.layers()[0], "inner_radius_fraction", high));
        EXPECT_FLOAT_EQ(particles.params().inner_radius_fraction, 1.0f);
    }
    for (float low : {-0.5f, -1e30f, -infinity, nan}) {
        visual_param::set(compositor.layers()[0], "inner_radius_fraction", 0.5f);
        EXPECT_TRUE(visual_param::set(compositor.layers()[0], "inner_radius_fraction", low));
        EXPECT_FLOAT_EQ(particles.params().inner_radius_fraction, 0.0f);
    }
}

TEST(VisualParamSetTest, BoolValueSentToParticlesInnerRadiusFractionIsRejectedNotCoerced) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);
    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor.layers()[0].visual);
    visual_param::set(compositor.layers()[0], "inner_radius_fraction", 0.35f);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "inner_radius_fraction", true));
    EXPECT_FLOAT_EQ(particles.params().inner_radius_fraction, 0.35f);
}

TEST(VisualParamSetTest, RadialOnlyOuterRadiusParamIsRejectedOnParticles) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);

    EXPECT_FALSE(visual_param::set(compositor.layers()[0], "max_radius_fraction", 0.5f));
}

namespace {

struct FractalFloatParamCase {
    const char* name;
    float FractalParams::* field;
    float in_range_value;
    float min;
    float max;
};

const FractalFloatParamCase kFractalFloatParamCases[] = {
    {"base_zoom", &FractalParams::base_zoom, 2.5f, visual_config::kFractalBaseZoomMin,
     visual_config::kFractalBaseZoomMax},
    {"zoom_amplitude_scale", &FractalParams::zoom_amplitude_scale, 1.25f,
     visual_config::kFractalZoomAmplitudeScaleMin, visual_config::kFractalZoomAmplitudeScaleMax},
    {"julia_base_radius", &FractalParams::julia_base_radius, 0.75f, visual_config::kFractalJuliaBaseRadiusMin,
     visual_config::kFractalJuliaBaseRadiusMax},
    {"julia_radius_amplitude_scale", &FractalParams::julia_radius_amplitude_scale, 0.12f,
     visual_config::kFractalJuliaRadiusAmplitudeScaleMin, visual_config::kFractalJuliaRadiusAmplitudeScaleMax},
    {"julia_angular_speed_radians_per_sec", &FractalParams::julia_angular_speed_radians_per_sec, 0.6f,
     visual_config::kFractalJuliaAngularSpeedRadiansPerSecMin,
     visual_config::kFractalJuliaAngularSpeedRadiansPerSecMax},
    {"julia_orbit_center_x", &FractalParams::julia_orbit_center_x, 0.2f,
     visual_config::kFractalJuliaOrbitCenterMin, visual_config::kFractalJuliaOrbitCenterMax},
    {"julia_orbit_center_y", &FractalParams::julia_orbit_center_y, -0.3f,
     visual_config::kFractalJuliaOrbitCenterMin, visual_config::kFractalJuliaOrbitCenterMax},
    {"color_cycle_speed_hz", &FractalParams::color_cycle_speed_hz, 0.3f,
     visual_config::kFractalColorCycleSpeedHzMin, visual_config::kFractalColorCycleSpeedHzMax},
};

Layer make_fractal_layer() { return Layer{VisualType::Fractal, std::make_unique<FractalVisual>()}; }

const FractalParams& fractal_params_of(const Layer& layer) {
    return dynamic_cast<FractalVisual&>(*layer.visual).params();
}

}

TEST(VisualParamSetTest, SetsEveryExposedFractalFloatParamAndLeavesTheOthersUntouched) {
    for (const FractalFloatParamCase& c : kFractalFloatParamCases) {
        Layer layer = make_fractal_layer();
        const FractalParams defaults;

        EXPECT_TRUE(visual_param::set(layer, c.name, c.in_range_value)) << c.name;

        EXPECT_FLOAT_EQ(fractal_params_of(layer).*(c.field), c.in_range_value) << c.name;
        for (const FractalFloatParamCase& other : kFractalFloatParamCases) {
            if (other.field == c.field) {
                continue;
            }
            EXPECT_FLOAT_EQ(fractal_params_of(layer).*(other.field), defaults.*(other.field))
                << c.name << " changed " << other.name;
        }
    }
}

TEST(VisualParamSetTest, ClampsEveryExposedFractalFloatParamIncludingExtremeAndNonFiniteValues) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    for (const FractalFloatParamCase& c : kFractalFloatParamCases) {
        Layer layer = make_fractal_layer();
        for (float high : {c.max + 0.25f, 1e30f, infinity}) {
            EXPECT_TRUE(visual_param::set(layer, c.name, high)) << c.name;
            EXPECT_FLOAT_EQ(fractal_params_of(layer).*(c.field), c.max) << c.name << " input " << high;
        }
        for (float low : {c.min - 0.25f, -1e30f, -infinity, nan}) {
            EXPECT_TRUE(visual_param::set(layer, c.name, low)) << c.name;
            EXPECT_FLOAT_EQ(fractal_params_of(layer).*(c.field), c.min) << c.name << " input " << low;
        }
    }
}

TEST(VisualParamSetTest, FractalBaseZoomCannotBeSetBelowOne) {
    Layer layer = make_fractal_layer();

    EXPECT_TRUE(visual_param::set(layer, "base_zoom", 0.5f));

    EXPECT_FLOAT_EQ(fractal_params_of(layer).base_zoom, 1.0f);
}

TEST(VisualParamSetTest, SetsFractalOrbitClockwiseToggleBothWays) {
    Layer layer = make_fractal_layer();
    ASSERT_TRUE(fractal_params_of(layer).julia_orbit_clockwise);

    EXPECT_TRUE(visual_param::set(layer, "julia_orbit_clockwise", false));
    EXPECT_FALSE(fractal_params_of(layer).julia_orbit_clockwise);

    EXPECT_TRUE(visual_param::set(layer, "julia_orbit_clockwise", true));
    EXPECT_TRUE(fractal_params_of(layer).julia_orbit_clockwise);
}

TEST(VisualParamSetTest, BoolValueSentToFractalFloatParamsIsRejectedNotCoerced) {
    for (const FractalFloatParamCase& c : kFractalFloatParamCases) {
        Layer layer = make_fractal_layer();
        const FractalParams defaults;

        EXPECT_FALSE(visual_param::set(layer, c.name, true)) << c.name;

        EXPECT_FLOAT_EQ(fractal_params_of(layer).*(c.field), defaults.*(c.field)) << c.name;
    }
}

TEST(VisualParamSetTest, FloatValueSentToFractalOrbitClockwiseIsRejectedNotCoerced) {
    Layer layer = make_fractal_layer();

    EXPECT_FALSE(visual_param::set(layer, "julia_orbit_clockwise", 0.0f));

    EXPECT_TRUE(fractal_params_of(layer).julia_orbit_clockwise);
}

TEST(VisualParamSetTest, UnexposedAndUnknownParamNamesAreRejectedOnFractal) {
    Layer layer = make_fractal_layer();
    const FractalParams defaults;

    EXPECT_FALSE(visual_param::set(layer, "amplitude_smoothing_tau_ms", 500.0f));
    EXPECT_FALSE(visual_param::set(layer, "boundary_margin_px", 40.0f));
    EXPECT_FALSE(visual_param::set(layer, "max_iterations", 500.0f));
    EXPECT_FALSE(visual_param::set(layer, "not_a_real_param", 1.0f));
    EXPECT_FALSE(visual_param::set(layer, "spawn_rate_per_frame", 1.0f));

    EXPECT_FLOAT_EQ(fractal_params_of(layer).amplitude_smoothing_tau_ms, defaults.amplitude_smoothing_tau_ms);
    EXPECT_FLOAT_EQ(fractal_params_of(layer).boundary_margin_px, defaults.boundary_margin_px);
}

TEST(VisualParamSetTest, FractalParamNamesAreRejectedOnOtherVisualTypes) {
    Compositor compositor;
    compositor.add_layer(VisualType::Particles);
    compositor.add_layer(VisualType::Lissajous);

    for (std::size_t i = 0; i < compositor.layers().size(); ++i) {
        EXPECT_FALSE(visual_param::set(compositor.layers()[i], "base_zoom", 2.0f));
        EXPECT_FALSE(visual_param::set(compositor.layers()[i], "julia_orbit_clockwise", false));
    }
}
