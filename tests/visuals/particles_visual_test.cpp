#include "visuals/particles/particles_visual.h"

#include <cmath>
#include <random>

#include <limits>

#include <gtest/gtest.h>

#include "audio/audio_config.h"

namespace {
constexpr float kEpsilon = 1e-3f;
}

TEST(ParticlesSpawnDirectionTest, IsAlwaysAUnitVector) {
    std::mt19937 rng(42);
    for (int i = 0; i < 50; ++i) {
        Vector2 dir = particles_math::spawn_direction(3, 0.35f, rng);
        const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        EXPECT_NEAR(length, 1.0f, kEpsilon);
    }
}

TEST(ParticlesSpawnDirectionTest, BandZeroPointsStraightUpWithNoJitter) {
    std::mt19937 rng(1);
    Vector2 dir = particles_math::spawn_direction(0, 0.0f, rng);
    EXPECT_NEAR(dir.x, 0.0f, kEpsilon);
    EXPECT_LT(dir.y, 0.0f);
}

TEST(ParticlesSpawnDirectionTest, BandsAreEvenlySpacedByAngleWithNoJitter) {
    std::mt19937 rng(1);
    Vector2 opposite = particles_math::spawn_direction(audio_config::kNumBands / 2, 0.0f, rng);
    EXPECT_NEAR(opposite.x, 0.0f, kEpsilon);
    EXPECT_GT(opposite.y, 0.0f);
}

TEST(ParticlesSpawnDirectionTest, StaysWithinJitterOfBandAngle) {
    std::mt19937 rng(7);
    for (int i = 0; i < 50; ++i) {
        Vector2 dir = particles_math::spawn_direction(0, 0.1f, rng);
        EXPECT_LT(std::fabs(dir.x), std::sin(0.1f) + kEpsilon);
    }
}

TEST(ParticlesSpawnDirectionTest, GlobalBandIgnoresGridAcrossManyTrials) {
    std::mt19937 rng(99);
    float min_angle = 1000.0f;
    float max_angle = -1000.0f;
    for (int i = 0; i < 500; ++i) {
        Vector2 dir = particles_math::spawn_direction(audio_config::kGlobalBandIndex, 0.1f, rng);
        const float angle = std::atan2(dir.y, dir.x);
        min_angle = std::min(min_angle, angle);
        max_angle = std::max(max_angle, angle);
    }
    EXPECT_GT(max_angle - min_angle, PI);
}

TEST(ParticlesBandInRangeTest, GlobalIsAlwaysInRange) {
    EXPECT_TRUE(particles_math::band_in_range(std::nullopt, 16));
    EXPECT_TRUE(particles_math::band_in_range(std::nullopt, 0));
}

TEST(ParticlesBandInRangeTest, LastValidBandIndexIsInRange) {
    EXPECT_TRUE(particles_math::band_in_range(15, 16));
}

TEST(ParticlesBandInRangeTest, BandIndexEqualToCountIsOutOfRange) {
    EXPECT_FALSE(particles_math::band_in_range(16, 16));
}

TEST(ParticlesSpawnPositionTest, ZeroInnerRadiusReturnsCenterUnchanged) {
    Vector2 pos = particles_math::spawn_position(Vector2{50.0f, 50.0f}, Vector2{1.0f, 0.0f}, 0.0f);
    EXPECT_NEAR(pos.x, 50.0f, kEpsilon);
    EXPECT_NEAR(pos.y, 50.0f, kEpsilon);
}

TEST(ParticlesSpawnPositionTest, OffsetsAlongDirectionByInnerRadius) {
    Vector2 pos = particles_math::spawn_position(Vector2{50.0f, 50.0f}, Vector2{0.0f, -1.0f}, 20.0f);
    EXPECT_NEAR(pos.x, 50.0f, kEpsilon);
    EXPECT_NEAR(pos.y, 30.0f, kEpsilon);
}

TEST(ClampInnerRadiusFractionTest, ClampsToUnitRange) {
    EXPECT_FLOAT_EQ(particles_math::clamp_inner_radius_fraction(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(particles_math::clamp_inner_radius_fraction(-0.2f), 0.0f);
    EXPECT_FLOAT_EQ(particles_math::clamp_inner_radius_fraction(1.5f), 1.0f);
}

TEST(ClampSpawnRatePerFrameTest, ClampsToConfiguredRange) {
    EXPECT_FLOAT_EQ(particles_math::clamp_spawn_rate_per_frame(5.0f), 5.0f);
    EXPECT_FLOAT_EQ(particles_math::clamp_spawn_rate_per_frame(-1.0f),
                    visual_config::kParticlesSpawnRatePerFrameMin);
    EXPECT_FLOAT_EQ(particles_math::clamp_spawn_rate_per_frame(1000.0f),
                    visual_config::kParticlesSpawnRatePerFrameMax);
}

TEST(ClampLifetimeFramesTest, ClampsToConfiguredRange) {
    EXPECT_EQ(particles_math::clamp_lifetime_frames(200), 200);
    EXPECT_EQ(particles_math::clamp_lifetime_frames(0), visual_config::kParticlesLifetimeFramesMin);
    EXPECT_EQ(particles_math::clamp_lifetime_frames(10000), visual_config::kParticlesLifetimeFramesMax);
}

TEST(ParticlesAccumulateSpawnCountTest, CarriesFractionalRemainderAcrossFrames) {
    float accumulator = 0.0f;
    EXPECT_EQ(particles_math::accumulate_spawn_count(accumulator, 0.3f), 0);
    EXPECT_EQ(particles_math::accumulate_spawn_count(accumulator, 0.3f), 0);
    EXPECT_EQ(particles_math::accumulate_spawn_count(accumulator, 0.3f), 0);
    EXPECT_EQ(particles_math::accumulate_spawn_count(accumulator, 0.3f), 1);
}

TEST(ParticlesAccumulateSpawnCountTest, HandlesRatesAboveOnePerFrame) {
    float accumulator = 0.0f;
    EXPECT_EQ(particles_math::accumulate_spawn_count(accumulator, 2.5f), 2);
    EXPECT_EQ(particles_math::accumulate_spawn_count(accumulator, 2.5f), 3);
    EXPECT_EQ(particles_math::accumulate_spawn_count(accumulator, 2.5f), 2);
}

TEST(ParticlesSpawnIntoPoolTest, ActivatesFirstInactiveSlotWithGivenState) {
    particles_math::Pool pool{};
    const bool spawned =
        particles_math::spawn_into_pool(pool, Vector2{10.0f, 20.0f}, Vector2{1.0f, -2.0f}, 30);

    EXPECT_TRUE(spawned);
    EXPECT_TRUE(pool[0].active);
    EXPECT_NEAR(pool[0].position.x, 10.0f, kEpsilon);
    EXPECT_NEAR(pool[0].position.y, 20.0f, kEpsilon);
    EXPECT_NEAR(pool[0].velocity.x, 1.0f, kEpsilon);
    EXPECT_NEAR(pool[0].velocity.y, -2.0f, kEpsilon);
    EXPECT_EQ(pool[0].age_frames, 0);
    EXPECT_EQ(pool[0].lifetime_frames, 30);
}

TEST(ParticlesSpawnIntoPoolTest, ReturnsFalseWhenPoolIsFull) {
    particles_math::Pool pool{};
    for (auto& p : pool) {
        p.active = true;
    }
    EXPECT_FALSE(particles_math::spawn_into_pool(pool, Vector2{0, 0}, Vector2{0, 0}, 10));
}

TEST(ParticlesStepPoolTest, AdvancesPositionAndAgeForActiveParticles) {
    particles_math::Pool pool{};
    pool[0].active = true;
    pool[0].position = Vector2{0.0f, 0.0f};
    pool[0].velocity = Vector2{1.0f, 2.0f};
    pool[0].age_frames = 0;
    pool[0].lifetime_frames = 5;

    particles_math::step_pool(pool, 200.0f, 200.0f, 3.0f);

    EXPECT_NEAR(pool[0].position.x, 1.0f, kEpsilon);
    EXPECT_NEAR(pool[0].position.y, 2.0f, kEpsilon);
    EXPECT_EQ(pool[0].age_frames, 1);
    EXPECT_TRUE(pool[0].active);
}

TEST(ParticlesStepPoolTest, DeactivatesOnceAgeReachesLifetime) {
    particles_math::Pool pool{};
    pool[0].active = true;
    pool[0].lifetime_frames = 1;
    pool[0].age_frames = 0;

    particles_math::step_pool(pool, 200.0f, 200.0f, 3.0f);

    EXPECT_FALSE(pool[0].active);
}

TEST(ParticlesStepPoolTest, IgnoresInactiveSlots) {
    particles_math::Pool pool{};
    pool[0].active = false;
    pool[0].position = Vector2{5.0f, 5.0f};
    pool[0].velocity = Vector2{1.0f, 2.0f};
    pool[0].age_frames = 0;
    pool[0].lifetime_frames = 5;

    particles_math::step_pool(pool, 200.0f, 200.0f, 3.0f);

    EXPECT_NEAR(pool[0].position.x, 5.0f, kEpsilon);
    EXPECT_NEAR(pool[0].position.y, 5.0f, kEpsilon);
    EXPECT_EQ(pool[0].age_frames, 0);
}

TEST(ParticlesStepPoolTest, DeactivatesImmediatelyOnceFullyOffscreenRegardlessOfLifetime) {
    particles_math::Pool pool{};
    pool[0].active = true;
    pool[0].position = Vector2{190.0f, 50.0f};
    pool[0].velocity = Vector2{20.0f, 0.0f};
    pool[0].age_frames = 0;
    pool[0].lifetime_frames = 1000;

    particles_math::step_pool(pool, 200.0f, 100.0f, 3.0f);

    EXPECT_FALSE(pool[0].active);
}

TEST(ParticlesStepPoolTest, StaysActiveWhileStillWithinMarginOfEdge) {
    particles_math::Pool pool{};
    pool[0].active = true;
    pool[0].position = Vector2{199.0f, 50.0f};
    pool[0].velocity = Vector2{2.0f, 0.0f};
    pool[0].age_frames = 0;
    pool[0].lifetime_frames = 1000;

    particles_math::step_pool(pool, 200.0f, 100.0f, 3.0f);

    EXPECT_TRUE(pool[0].active);
}

TEST(ParticlesStepPoolTest, NegativeYSideOffscreenAlsoDeactivates) {
    particles_math::Pool pool{};
    pool[0].active = true;
    pool[0].position = Vector2{10.0f, 5.0f};
    pool[0].velocity = Vector2{0.0f, -20.0f};
    pool[0].age_frames = 0;
    pool[0].lifetime_frames = 1000;

    particles_math::step_pool(pool, 200.0f, 100.0f, 3.0f);

    EXPECT_FALSE(pool[0].active);
}

TEST(ParticlesStepPoolTest, NegativeXSideOffscreenAlsoDeactivates) {
    particles_math::Pool pool{};
    pool[0].active = true;
    pool[0].position = Vector2{5.0f, 10.0f};
    pool[0].velocity = Vector2{-20.0f, 0.0f};
    pool[0].age_frames = 0;
    pool[0].lifetime_frames = 1000;

    particles_math::step_pool(pool, 200.0f, 100.0f, 3.0f);

    EXPECT_FALSE(pool[0].active);
}

TEST(ParticlesStepPoolTest, PositiveYSideOffscreenAlsoDeactivates) {
    particles_math::Pool pool{};
    pool[0].active = true;
    pool[0].position = Vector2{10.0f, 90.0f};
    pool[0].velocity = Vector2{0.0f, 20.0f};
    pool[0].age_frames = 0;
    pool[0].lifetime_frames = 1000;

    particles_math::step_pool(pool, 200.0f, 100.0f, 3.0f);

    EXPECT_FALSE(pool[0].active);
}

TEST(ParticlesFadedColorTest, FullAlphaAtSpawn) {
    Color c = particles_math::faded_color(WHITE, 0, 72);
    EXPECT_EQ(c.a, 255);
}

TEST(ParticlesFadedColorTest, FullyTransparentAtLifetimeEnd) {
    Color c = particles_math::faded_color(WHITE, 72, 72);
    EXPECT_EQ(c.a, 0);
}

TEST(ParticlesFadedColorTest, LinearlyInterpolatesMidLifetime) {
    Color c = particles_math::faded_color(WHITE, 36, 72);
    EXPECT_NEAR(static_cast<int>(c.a), 127, 2);
}

TEST(ParticlesAmplitudeSpawnRatioTest, ZeroWhenIdleRegardlessOfEnergyOrReference) {
    EXPECT_FLOAT_EQ(particles_math::amplitude_spawn_ratio(500.0f, 100.0f, true), 0.0f);
    EXPECT_FLOAT_EQ(particles_math::amplitude_spawn_ratio(0.05f, 0.1f, true), 0.0f);
}

TEST(ParticlesAmplitudeSpawnRatioTest, ZeroSpawnRateWhenIdleRegardlessOfSensitivity) {
    const float ratio = particles_math::amplitude_spawn_ratio(50.0f, 100.0f, true);
    for (float sensitivity : {0.0f, 1.0f, 2.5f, 4.0f}) {
        const float rate = 2.0f * ratio * sensitivity;
        ASSERT_TRUE(std::isfinite(sensitivity));
        EXPECT_FLOAT_EQ(rate, 0.0f);
    }
}

TEST(ParticlesAmplitudeSpawnRatioTest, NormalizesAndClampsWhenNotIdle) {
    EXPECT_FLOAT_EQ(particles_math::amplitude_spawn_ratio(50.0f, 100.0f, false), 0.5f);
    EXPECT_FLOAT_EQ(particles_math::amplitude_spawn_ratio(200.0f, 100.0f, false), 1.0f);
}

TEST(ParticlesAmplitudeSpawnRatioTest, NonPositiveReferenceYieldsZeroEvenWhenNotIdle) {
    EXPECT_FLOAT_EQ(particles_math::amplitude_spawn_ratio(5.0f, 0.0f, false), 0.0f);
    EXPECT_FLOAT_EQ(particles_math::amplitude_spawn_ratio(5.0f, -1.0f, false), 0.0f);
}

TEST(ParticlesFadedColorTest, PreservesRgbChannels) {
    Color base{10, 20, 30, 200};
    Color c = particles_math::faded_color(base, 0, 72);
    EXPECT_EQ(c.r, 10);
    EXPECT_EQ(c.g, 20);
    EXPECT_EQ(c.b, 30);
}

TEST(ParticlesBaseColorTest, NoOverrideReturnsFlatColor) {
    Color c = particles_math::base_color(RED, std::nullopt, 10, 100);
    EXPECT_EQ(c.r, RED.r);
    EXPECT_EQ(c.g, RED.g);
    EXPECT_EQ(c.b, RED.b);
}

TEST(ParticlesBaseColorTest, OverrideSamplesByLifeFraction) {
    Palette palette{{{0.0f, Color{0, 0, 0, 255}}, {1.0f, Color{200, 0, 0, 255}}}};
    Color newborn = particles_math::base_color(WHITE, palette, 0, 100);
    Color mid = particles_math::base_color(WHITE, palette, 50, 100);
    Color old = particles_math::base_color(WHITE, palette, 100, 100);
    EXPECT_EQ(newborn.r, 0);
    EXPECT_NEAR(mid.r, 100, 1);
    EXPECT_EQ(old.r, 200);
}

TEST(ParticlesBaseColorTest, ZeroLifetimeNormalizesToZero) {
    Palette palette{{{0.0f, Color{0, 0, 0, 255}}, {1.0f, Color{200, 0, 0, 255}}}};
    Color c = particles_math::base_color(WHITE, palette, 0, 0);
    EXPECT_EQ(c.r, 0);
}

TEST(ParticlesClampLifetimeFromNumberTest, ExtremeAndNonFiniteValuesClampWithoutReachingTheIntCast) {
    EXPECT_EQ(particles_math::clamp_lifetime_frames_from_number(1e30),
              visual_config::kParticlesLifetimeFramesMax);
    EXPECT_EQ(particles_math::clamp_lifetime_frames_from_number(-1e30),
              visual_config::kParticlesLifetimeFramesMin);
    EXPECT_EQ(particles_math::clamp_lifetime_frames_from_number(std::numeric_limits<double>::infinity()),
              visual_config::kParticlesLifetimeFramesMax);
    EXPECT_EQ(particles_math::clamp_lifetime_frames_from_number(-std::numeric_limits<double>::infinity()),
              visual_config::kParticlesLifetimeFramesMin);
    EXPECT_EQ(particles_math::clamp_lifetime_frames_from_number(std::numeric_limits<double>::quiet_NaN()),
              visual_config::kParticlesLifetimeFramesMin);
}

TEST(ParticlesClampLifetimeFromNumberTest, InRangeValueTruncatesToInt) {
    EXPECT_EQ(particles_math::clamp_lifetime_frames_from_number(150.9), 150);
}
