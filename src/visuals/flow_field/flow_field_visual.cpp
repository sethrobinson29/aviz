#include "visuals/flow_field/flow_field_visual.h"

#include <algorithm>
#include <cmath>

#include "audio/audio_config.h"
#include "visuals/visual_config.h"

namespace flow_field_math {

namespace {

float hash_to_unit(std::uint32_t h) {
    return static_cast<float>(h & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

float smoothstep(float t) { return t * t * (3.0f - 2.0f * t); }

}

std::uint32_t hash2d(int x, int y) {
    std::uint32_t h =
        static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float value_noise2d(float x, float y) {
    int ix = static_cast<int>(std::floor(x));
    int iy = static_cast<int>(std::floor(y));
    float fx = x - static_cast<float>(ix);
    float fy = y - static_cast<float>(iy);

    float v00 = hash_to_unit(hash2d(ix, iy));
    float v10 = hash_to_unit(hash2d(ix + 1, iy));
    float v01 = hash_to_unit(hash2d(ix, iy + 1));
    float v11 = hash_to_unit(hash2d(ix + 1, iy + 1));

    float u = smoothstep(fx);
    float v = smoothstep(fy);

    float top = v00 + (v10 - v00) * u;
    float bottom = v01 + (v11 - v01) * u;
    float value = top + (bottom - top) * v;
    return value * 2.0f - 1.0f;
}

Vector2 field_direction(float x, float y, float turbulence_frequency, float time_phase) {
    float n = value_noise2d(x * turbulence_frequency + time_phase, y * turbulence_frequency);
    float angle = n * PI;
    return Vector2{std::cos(angle), std::sin(angle)};
}

Vector2 random_canvas_position(float width, float height, std::mt19937& rng) {
    std::uniform_real_distribution<float> x_dist(0.0f, width);
    std::uniform_real_distribution<float> y_dist(0.0f, height);
    return Vector2{x_dist(rng), y_dist(rng)};
}

float clamp_spawn_rate_per_frame(float rate) {
    return std::clamp(rate, visual_config::kFlowFieldSpawnRatePerFrameMin,
                       visual_config::kFlowFieldSpawnRatePerFrameMax);
}

float clamp_turbulence_frequency(float frequency) {
    return std::clamp(frequency, visual_config::kFlowFieldTurbulenceFrequencyMin,
                       visual_config::kFlowFieldTurbulenceFrequencyMax);
}

float clamp_evolution_per_frame(float evolution) {
    return std::clamp(evolution, visual_config::kFlowFieldEvolutionPerFrameMin,
                       visual_config::kFlowFieldEvolutionPerFrameMax);
}

int clamp_grid_cols(int cols) {
    return std::clamp(cols, visual_config::kFlowFieldGridColsMin, visual_config::kFlowFieldGridColsMax);
}

int clamp_grid_cols_from_number(double cols) {
    if (!(cols >= static_cast<double>(visual_config::kFlowFieldGridColsMin))) {
        return visual_config::kFlowFieldGridColsMin;
    }
    if (cols >= static_cast<double>(visual_config::kFlowFieldGridColsMax)) {
        return visual_config::kFlowFieldGridColsMax;
    }
    return static_cast<int>(cols);
}

int grid_rows_for_cols(int cols) {
    const int rounded = (cols * visual_config::kFlowFieldGridRows + visual_config::kFlowFieldGridCols / 2) /
                        visual_config::kFlowFieldGridCols;
    return std::clamp(rounded, 1, visual_config::kFlowFieldGridRows);
}

std::size_t grid_cell_index(float x, float y, float cell_width, float cell_height, int cols, int rows) {
    int col = static_cast<int>(std::floor(x / cell_width));
    int row = static_cast<int>(std::floor(y / cell_height));
    col = std::clamp(col, 0, cols - 1);
    row = std::clamp(row, 0, rows - 1);
    return static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) + static_cast<std::size_t>(col);
}

Vector2 steer_velocity(Vector2 current_velocity, Vector2 target_direction, float speed, float steer_factor) {
    float current_len =
        std::sqrt(current_velocity.x * current_velocity.x + current_velocity.y * current_velocity.y);
    Vector2 current_dir = current_len > 0.0f
                               ? Vector2{current_velocity.x / current_len, current_velocity.y / current_len}
                               : target_direction;

    float t = std::clamp(steer_factor, 0.0f, 1.0f);
    Vector2 blended{current_dir.x + (target_direction.x - current_dir.x) * t,
                     current_dir.y + (target_direction.y - current_dir.y) * t};
    float blended_len = std::sqrt(blended.x * blended.x + blended.y * blended.y);
    Vector2 blended_dir = blended_len > 0.0f
                               ? Vector2{blended.x / blended_len, blended.y / blended_len}
                               : target_direction;
    return Vector2{blended_dir.x * speed, blended_dir.y * speed};
}

float ripple_influence(float distance_from_origin, float ring_radius, float bandwidth) {
    if (bandwidth <= 0.0f) {
        return 0.0f;
    }
    float delta = std::fabs(distance_from_origin - ring_radius);
    return std::clamp(1.0f - delta / bandwidth, 0.0f, 1.0f);
}

Vector2 ripple_impulse(Vector2 particle_position, Vector2 ripple_origin, float ring_radius,
                        float bandwidth, float strength) {
    Vector2 delta{particle_position.x - ripple_origin.x, particle_position.y - ripple_origin.y};
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (distance <= 0.0f) {
        return Vector2{0.0f, 0.0f};
    }
    float influence = ripple_influence(distance, ring_radius, bandwidth);
    if (influence <= 0.0f) {
        return Vector2{0.0f, 0.0f};
    }
    Vector2 dir{delta.x / distance, delta.y / distance};
    float magnitude = strength * influence;
    return Vector2{dir.x * magnitude, dir.y * magnitude};
}

}

FlowFieldVisual::FlowFieldVisual(FlowFieldParams params) : params_(params) {}

FlowFieldVisual::~FlowFieldVisual() {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
}

void FlowFieldVisual::resize(int width, int height) {
    if (width_ > 0 && height_ > 0) {
        UnloadRenderTexture(texture_);
    }
    texture_ = LoadRenderTexture(width, height);
    width_ = width;
    height_ = height;
}

void FlowFieldVisual::render(const AudioFeatures& features, const LayerReactivity& reactivity,
                              const std::optional<Palette>& palette_override) {
    const bool in_range = particles_math::band_in_range(reactivity.freq_band, audio_config::kNumBands);
    const std::size_t band = reactivity.freq_band.value_or(audio_config::kGlobalBandIndex);

    const float ratio = in_range ? particles_math::amplitude_spawn_ratio(
                                        features.band_energy[band], features.band_agc_reference[band],
                                        features.band_idle[band])
                                  : 0.0f;
    const bool onset = in_range && features.band_onset[band];

    const float turbulence_frequency =
        params_.turbulence_frequency * (1.0f + ratio * reactivity.sensitivity);
    const float evolution_step =
        params_.evolution_per_frame * (1.0f + ratio * reactivity.sensitivity);
    field_time_ += evolution_step;

    const int cols = flow_field_math::clamp_grid_cols(params_.grid_cols);
    const int rows = flow_field_math::grid_rows_for_cols(cols);
    const float cell_width = static_cast<float>(width_) / static_cast<float>(cols);
    const float cell_height = static_cast<float>(height_) / static_cast<float>(rows);
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            const float cx = (static_cast<float>(col) + 0.5f) * cell_width;
            const float cy = (static_cast<float>(row) + 0.5f) * cell_height;
            grid_[static_cast<std::size_t>(row) * static_cast<std::size_t>(cols) +
                  static_cast<std::size_t>(col)] =
                flow_field_math::field_direction(cx, cy, turbulence_frequency, field_time_);
        }
    }

    if (in_range) {
        const Vector2 center{width_ * 0.5f, height_ * 0.5f};

        const float rate = params_.spawn_rate_per_frame * ratio * reactivity.sensitivity;
        const int count = particles_math::accumulate_spawn_count(spawn_accumulator_, rate);
        for (int i = 0; i < count; ++i) {
            const Vector2 origin = flow_field_math::random_canvas_position(
                static_cast<float>(width_), static_cast<float>(height_), rng_);
            const std::size_t cell =
                flow_field_math::grid_cell_index(origin.x, origin.y, cell_width, cell_height, cols, rows);
            const Vector2 velocity{grid_[cell].x * visual_config::kFlowFieldSpeedPxPerFrame,
                                    grid_[cell].y * visual_config::kFlowFieldSpeedPxPerFrame};
            particles_math::spawn_into_pool(pool_, origin, velocity, params_.lifetime_frames);
        }

        if (onset && params_.burst_on_onset_enabled) {
            for (int i = 0; i < params_.burst_count; ++i) {
                const Vector2 origin = flow_field_math::random_canvas_position(
                    static_cast<float>(width_), static_cast<float>(height_), rng_);
                const std::size_t cell = flow_field_math::grid_cell_index(origin.x, origin.y, cell_width,
                                                                           cell_height, cols, rows);
                const Vector2 velocity{grid_[cell].x * visual_config::kFlowFieldSpeedPxPerFrame,
                                        grid_[cell].y * visual_config::kFlowFieldSpeedPxPerFrame};
                particles_math::spawn_into_pool(pool_, origin, velocity, params_.lifetime_frames);
            }
        }

        if (onset && params_.ripple_on_onset_enabled) {
            for (Ripple& ripple : ripples_) {
                if (!ripple.active) {
                    ripple.origin = center;
                    ripple.age_frames = 0;
                    ripple.active = true;
                    break;
                }
            }
        }
    }

    for (Ripple& ripple : ripples_) {
        if (!ripple.active) {
            continue;
        }
        ripple.age_frames += 1;
        if (ripple.age_frames >= visual_config::kFlowFieldRippleLifetimeFrames) {
            ripple.active = false;
        }
    }

    for (particles_math::Particle& p : pool_) {
        if (!p.active) {
            continue;
        }
        const std::size_t cell =
            flow_field_math::grid_cell_index(p.position.x, p.position.y, cell_width, cell_height, cols, rows);
        Vector2 new_velocity = flow_field_math::steer_velocity(
            p.velocity, grid_[cell], visual_config::kFlowFieldSpeedPxPerFrame,
            visual_config::kFlowFieldSteerFactor);
        for (const Ripple& ripple : ripples_) {
            if (!ripple.active) {
                continue;
            }
            const float ring_radius =
                static_cast<float>(ripple.age_frames) * visual_config::kFlowFieldRippleSpeedPxPerFrame;
            const Vector2 impulse = flow_field_math::ripple_impulse(
                p.position, ripple.origin, ring_radius, visual_config::kFlowFieldRippleBandwidthPx,
                visual_config::kFlowFieldRippleStrength * reactivity.sensitivity);
            new_velocity.x += impulse.x;
            new_velocity.y += impulse.y;
        }
        p.velocity = new_velocity;
    }

    particles_math::step_pool(pool_, static_cast<float>(width_), static_cast<float>(height_), params_.radius);

    BeginTextureMode(texture_);
    ClearBackground(BLANK);
    for (const particles_math::Particle& p : pool_) {
        if (!p.active) {
            continue;
        }
        const Vector2 tail{p.position.x - p.velocity.x, p.position.y - p.velocity.y};
        const Color base = particles_math::base_color(params_.particle_color, palette_override, p.age_frames,
                                                        p.lifetime_frames);
        DrawLineEx(tail, p.position, params_.radius, particles_math::faded_color(base, p.age_frames, p.lifetime_frames));
    }
    EndTextureMode();
}
