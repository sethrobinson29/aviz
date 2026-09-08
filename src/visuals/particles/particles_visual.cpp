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

bool band_in_range(std::optional<std::size_t> freq_band, std::size_t band_count) {
    return !freq_band.has_value() || *freq_band < band_count;
}

Vector2 spawn_position(Vector2 center, Vector2 direction, float inner_radius) {
    return Vector2{center.x + direction.x * inner_radius, center.y + direction.y * inner_radius};
}

float clamp_inner_radius_fraction(float inner) {
    return std::clamp(inner, 0.0f, 1.0f);
}

int accumulate_spawn_count(float& accumulator, float rate_per_frame) {
    accumulator += rate_per_frame;
    const float count = std::floor(accumulator);
    accumulator -= count;
    return static_cast<int>(count);
}

float amplitude_spawn_ratio(float energy, float reference, bool idle) {
    if (idle || reference <= 0.0f) {
        return 0.0f;
    }
    return std::clamp(energy / reference, 0.0f, 1.0f);
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

void step_pool(Pool& pool, float width, float height, float margin) {
    for (Particle& p : pool) {
        if (!p.active) {
            continue;
        }
        p.position.x += p.velocity.x;
        p.position.y += p.velocity.y;
        p.age_frames += 1;

        const bool aged_out = p.age_frames >= p.lifetime_frames;
        const bool fully_offscreen = p.position.x < -margin || p.position.x > width + margin ||
                                      p.position.y < -margin || p.position.y > height + margin;
        if (aged_out || fully_offscreen) {
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

}

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
    const bool band_in_range =
        particles_math::band_in_range(reactivity.freq_band, audio_config::kNumBands);
    const std::size_t band = reactivity.freq_band.value_or(audio_config::kGlobalBandIndex);

    if (band_in_range) {
        const Vector2 center{width_ * 0.5f, height_ * 0.5f};
        const float unit_radius = std::min(width_, height_) * 0.5f;
        const float inner_radius = unit_radius * params_.inner_radius_fraction;
        const bool onset_mode = reactivity.source == ReactivitySource::BandOnset ||
                                 reactivity.source == ReactivitySource::GlobalBeat;

        if (onset_mode) {
            if (features.band_onset[band]) {
                const float speed = visual_config::kParticleBaseSpeedPxPerFrame * reactivity.sensitivity;
                for (int i = 0; i < params_.burst_count; ++i) {
                    const Vector2 dir = particles_math::spawn_direction(
                        band, visual_config::kParticleAngleJitterRadians, rng_);
                    const Vector2 origin = particles_math::spawn_position(center, dir, inner_radius);
                    particles_math::spawn_into_pool(pool_, origin, Vector2{dir.x * speed, dir.y * speed},
                                                     params_.lifetime_frames);
                }
            }
        } else {
            const float ratio = particles_math::amplitude_spawn_ratio(
                features.band_energy[band], features.band_agc_reference[band], features.band_idle[band]);
            const float rate = params_.spawn_rate_per_frame * ratio * reactivity.sensitivity;
            const int count = particles_math::accumulate_spawn_count(spawn_accumulator_, rate);
            const float speed =
                visual_config::kParticleBaseSpeedPxPerFrame * (1.0f + ratio * reactivity.sensitivity);
            for (int i = 0; i < count; ++i) {
                const Vector2 dir = particles_math::spawn_direction(
                    band, visual_config::kParticleAngleJitterRadians, rng_);
                const Vector2 origin = particles_math::spawn_position(center, dir, inner_radius);
                particles_math::spawn_into_pool(pool_, origin, Vector2{dir.x * speed, dir.y * speed},
                                                 params_.lifetime_frames);
            }
        }
    }

    particles_math::step_pool(pool_, static_cast<float>(width_), static_cast<float>(height_),
                               params_.radius);

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
