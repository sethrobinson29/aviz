#include "compositor/visual_param.h"

#include <gtest/gtest.h>

#include "compositor/compositor.h"
#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/particles/particles_visual.h"
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
