#include "compositor/visual_param.h"

#include <gtest/gtest.h>

#include "compositor/compositor.h"
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
