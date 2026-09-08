#include "visuals/particles/particles_visual.h"

#include <cmath>
#include <random>

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
    // Band kNumBands/2 sits 180 degrees from band 0 -- mirrored on both axes.
    Vector2 opposite = particles_math::spawn_direction(audio_config::kNumBands / 2, 0.0f, rng);
    EXPECT_NEAR(opposite.x, 0.0f, kEpsilon);
    EXPECT_GT(opposite.y, 0.0f);
}

TEST(ParticlesSpawnDirectionTest, StaysWithinJitterOfBandAngle) {
    std::mt19937 rng(7);
    // Band 0's base angle is straight up (-90 degrees); with jitter 0.1 rad,
    // x should stay small (sin of a small angle) rather than spanning the
    // full circle.
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
    // A grid-restricted draw (even with jitter) couldn't span more than
    // ~half the circle; global scope should easily exceed that over 500
    // uniform draws across the full circle.
    EXPECT_GT(max_angle - min_angle, PI);
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
    EXPECT_EQ(particles_math::accumulate_spawn_count(accumulator, 2.5f), 2);  // 2.5 -> 2, rem 0.5
    EXPECT_EQ(particles_math::accumulate_spawn_count(accumulator, 2.5f), 3);  // 3.0 -> 3, rem 0
    EXPECT_EQ(particles_math::accumulate_spawn_count(accumulator, 2.5f), 2);  // 2.5 -> 2, rem 0.5
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

    particles_math::step_pool(pool);

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

    particles_math::step_pool(pool);

    EXPECT_FALSE(pool[0].active);
}

TEST(ParticlesStepPoolTest, IgnoresInactiveSlots) {
    particles_math::Pool pool{};
    pool[0].active = false;
    pool[0].position = Vector2{5.0f, 5.0f};

    particles_math::step_pool(pool);

    EXPECT_NEAR(pool[0].position.x, 5.0f, kEpsilon);
    EXPECT_NEAR(pool[0].position.y, 5.0f, kEpsilon);
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

TEST(ParticlesFadedColorTest, PreservesRgbChannels) {
    Color base{10, 20, 30, 200};
    Color c = particles_math::faded_color(base, 0, 72);
    EXPECT_EQ(c.r, 10);
    EXPECT_EQ(c.g, 20);
    EXPECT_EQ(c.b, 30);
}
