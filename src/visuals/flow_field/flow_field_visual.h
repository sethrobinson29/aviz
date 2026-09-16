#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>

#include "raylib.h"
#include "visuals/particles/particles_visual.h"
#include "visuals/visual_config.h"
#include "visuals/visual_module.h"

namespace flow_field_math {

std::uint32_t hash2d(int x, int y);

float value_noise2d(float x, float y);

Vector2 field_direction(float x, float y, float turbulence_frequency, float time_phase);

Vector2 random_canvas_position(float width, float height, std::mt19937& rng);

std::size_t grid_cell_index(float x, float y, float cell_width, float cell_height, int cols, int rows);

Vector2 steer_velocity(Vector2 current_velocity, Vector2 target_direction, float speed,
                        float steer_factor);

float ripple_influence(float distance_from_origin, float ring_radius, float bandwidth);

Vector2 ripple_impulse(Vector2 particle_position, Vector2 ripple_origin, float ring_radius,
                        float bandwidth, float strength);

}

struct FlowFieldParams {
    Color particle_color = WHITE;
    float radius = 2.0f;
    int lifetime_frames = visual_config::kFlowFieldDefaultLifetimeFrames;
    int burst_count = visual_config::kFlowFieldDefaultBurstCount;
    float spawn_rate_per_frame = visual_config::kFlowFieldDefaultSpawnRatePerFrame;
    bool ripple_on_onset_enabled = true;
    bool burst_on_onset_enabled = true;
};

class FlowFieldVisual : public VisualModule {
public:
    explicit FlowFieldVisual(FlowFieldParams params = {});
    ~FlowFieldVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features, const LayerReactivity& reactivity,
                const std::optional<Palette>& palette_override) override;
    RenderTexture2D& render_texture() override { return texture_; }
    std::unique_ptr<VisualModule> clone() const override {
        return std::make_unique<FlowFieldVisual>(params_);
    }
    const FlowFieldParams& params() const { return params_; }
    FlowFieldParams& mutable_params() { return params_; }

private:
    struct Ripple {
        Vector2 origin{};
        int age_frames = 0;
        bool active = false;
    };

    FlowFieldParams params_;
    RenderTexture2D texture_{};
    int width_ = 0;
    int height_ = 0;
    particles_math::Pool pool_{};
    float spawn_accumulator_ = 0.0f;
    std::mt19937 rng_{std::random_device{}()};
    float field_time_ = 0.0f;
    std::array<Vector2, visual_config::kFlowFieldMaxGridCells> grid_{};
    std::array<Ripple, visual_config::kMaxFlowFieldRipples> ripples_{};
};
