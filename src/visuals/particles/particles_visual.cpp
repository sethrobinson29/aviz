#include "visuals/particles/particles_visual.h"

#include <algorithm>
#include <cmath>

#include "audio/audio_config.h"
#include "visuals/visual_config.h"

namespace particles_math {

Vector2 spawn_direction(std::size_t band_index, float jitter_radians, std::mt19937& rng) {
    std::uniform_real_distribution<float> jitter(-jitter_radians, jitter_radians);

    float angle;
    if (band_index == audio_config::kGlobalBandIndex) {
        std::uniform_real_distribution<float> full_circle(0.0f, 2.0f * PI);
        angle = full_circle(rng);
    } else {
        const float angle_step = 2.0f * PI / static_cast<float>(audio_config::kNumBands);
        angle = -PI * 0.5f + static_cast<float>(band_index) * angle_step + jitter(rng);
    }
    return Vector2{std::cos(angle), std::sin(angle)};
}

int accumulate_spawn_count(float& accumulator, float rate_per_frame) {
    accumulator += rate_per_frame;
    const float count = std::floor(accumulator);
    accumulator -= count;
    return static_cast<int>(count);
}

bool spawn_into_pool(Pool& pool, Vector2 position, Vector2 velocity, int lifetime_frames) {
    for (Particle& p : pool) {
        if (!p.active) {
            p.position = position;
            p.velocity = velocity;
            p.age_frames = 0;
            p.lifetime_frames = lifetime_frames;
            p.active = true;
            return true;
        }
    }
    return false;
}

void step_pool(Pool& pool) {
    for (Particle& p : pool) {
        if (!p.active) {
            continue;
        }
        p.position.x += p.velocity.x;
        p.position.y += p.velocity.y;
        p.age_frames += 1;
        if (p.age_frames >= p.lifetime_frames) {
            p.active = false;
        }
    }
}

Color faded_color(Color base, int age_frames, int lifetime_frames) {
    float fraction = 0.0f;
    if (lifetime_frames > 0) {
        fraction = 1.0f - static_cast<float>(age_frames) / static_cast<float>(lifetime_frames);
    }
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    const unsigned char alpha =
        static_cast<unsigned char>(std::clamp(static_cast<float>(base.a) * fraction, 0.0f, 255.0f));
    return Color{base.r, base.g, base.b, alpha};
}

}  // namespace particles_math

ParticlesVisual::ParticlesVisual(ParticlesParams params) : params_(params) {}

ParticlesVisual::~ParticlesVisual() {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
}

void ParticlesVisual::resize(int width, int height) {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
    texture_ = LoadRenderTexture(width, height);
    width_ = width;
    height_ = height;
}

void ParticlesVisual::render(const AudioFeatures& features, const LayerReactivity& reactivity) {
    // Out-of-range freq_band gets no emitter this frame, same handling as
    // spectrum_math::bar_indices_to_draw's out-of-range case -- existing
    // particles still step/draw/fade out normally.
    const bool band_in_range =
        !reactivity.freq_band.has_value() || *reactivity.freq_band < audio_config::kNumBands;
    const std::size_t band = reactivity.freq_band.value_or(audio_config::kGlobalBandIndex);

    if (band_in_range) {
        const Vector2 center{width_ * 0.5f, height_ * 0.5f};
        const bool onset_mode = reactivity.source == ReactivitySource::BandOnset ||
                                 reactivity.source == ReactivitySource::GlobalBeat;

        if (onset_mode) {
            if (features.band_onset[band]) {
                const float speed = visual_config::kParticleBaseSpeedPxPerFrame * reactivity.sensitivity;
                for (int i = 0; i < params_.burst_count; ++i) {
                    const Vector2 dir = particles_math::spawn_direction(
                        band, visual_config::kParticleAngleJitterRadians, rng_);
                    particles_math::spawn_into_pool(pool_, center, Vector2{dir.x * speed, dir.y * speed},
                                                     params_.lifetime_frames);
                }
            }
        } else {
            float ratio = 0.0f;
            if (features.band_agc_reference[band] > 0.0f) {
                ratio = features.band_energy[band] / features.band_agc_reference[band];
            }
            const float rate = params_.spawn_rate_per_frame * ratio * reactivity.sensitivity;
            const int count = particles_math::accumulate_spawn_count(spawn_accumulator_, rate);
            const float speed =
                visual_config::kParticleBaseSpeedPxPerFrame * (1.0f + ratio * reactivity.sensitivity);
            for (int i = 0; i < count; ++i) {
                const Vector2 dir = particles_math::spawn_direction(
                    band, visual_config::kParticleAngleJitterRadians, rng_);
                particles_math::spawn_into_pool(pool_, center, Vector2{dir.x * speed, dir.y * speed},
                                                 params_.lifetime_frames);
            }
        }
    }

    particles_math::step_pool(pool_);

    BeginTextureMode(texture_);
    ClearBackground(BLANK);
    for (const particles_math::Particle& p : pool_) {
        if (!p.active) {
            continue;
        }
        DrawCircleV(p.position, params_.radius,
                    particles_math::faded_color(params_.particle_color, p.age_frames, p.lifetime_frames));
    }
    EndTextureMode();
}
