#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <random>

#include "audio/audio_config.h"
#include "raylib.h"
#include "visuals/visual_config.h"
#include "visuals/visual_module.h"

namespace particles_math {

struct Particle {
    Vector2 position{};
    Vector2 velocity{};
    int age_frames = 0;
    int lifetime_frames = 0;
    bool active = false;
};

using Pool = std::array<Particle, visual_config::kMaxParticles>;

Vector2 spawn_direction(std::size_t band_index, float jitter_radians, std::mt19937& rng);

bool band_in_range(std::optional<std::size_t> freq_band, std::size_t band_count);

Vector2 spawn_position(Vector2 center, Vector2 direction, float inner_radius);

float clamp_inner_radius_fraction(float inner);

float clamp_spawn_rate_per_frame(float rate);

int clamp_lifetime_frames(int frames);

int accumulate_spawn_count(float& accumulator, float rate_per_frame);

float amplitude_spawn_ratio(float energy, float reference, bool idle);

bool spawn_into_pool(Pool& pool, Vector2 position, Vector2 velocity, int lifetime_frames);

void step_pool(Pool& pool, float width, float height, float margin);

Color faded_color(Color base, int age_frames, int lifetime_frames);

Color base_color(Color flat_color, const std::optional<Palette>& palette_override, int age_frames,
                  int lifetime_frames);

}

struct ParticlesParams {
    Color particle_color = WHITE;
    float radius = 3.0f;
    int lifetime_frames = visual_config::kParticleDefaultLifetimeFrames;
    int burst_count = 24;
    float spawn_rate_per_frame = 2.0f;
    float inner_radius_fraction = 0.0f;
};

class ParticlesVisual : public VisualModule {
public:
    explicit ParticlesVisual(ParticlesParams params = {});
    ~ParticlesVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features, const LayerReactivity& reactivity,
                const std::optional<Palette>& palette_override) override;
    RenderTexture2D& render_texture() override { return texture_; }
    std::unique_ptr<VisualModule> clone() const override {
        return std::make_unique<ParticlesVisual>(params_);
    }
    const ParticlesParams& params() const { return params_; }
    ParticlesParams& mutable_params() { return params_; }

private:
    ParticlesParams params_;
    RenderTexture2D texture_{};
    int width_ = 0;
    int height_ = 0;
    particles_math::Pool pool_{};
    float spawn_accumulator_ = 0.0f;
    std::mt19937 rng_{std::random_device{}()};
};
