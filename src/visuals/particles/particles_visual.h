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

// Spawn direction for a burst/emission scoped to `band_index`: the same
// 12-o'clock, clockwise, evenly-spaced-across-kNumBands angle grid
// radial_math::to_radial_bars uses, so a band-scoped Particles layer's
// spawn direction lines up with a band-scoped Radial layer's segment at the
// same index, plus uniform random jitter of +/-jitter_radians. band_index
// == audio_config::kGlobalBandIndex ignores the grid and returns a
// uniformly random direction across the full circle instead.
Vector2 spawn_direction(std::size_t band_index, float jitter_radians, std::mt19937& rng);

bool band_in_range(std::optional<std::size_t> freq_band, std::size_t band_count);

// Spawn origin offset from `center` by `inner_radius` along `direction` --
// the same "start away from dead center" placement radial_math::to_radial_bars
// uses for a segment's inner endpoint, but for Particles there's no paired
// outer bound to stay clear of (off-canvas cleanup + lifetime already govern
// how far a particle travels), so this is a single point, not a segment, and
// inner_radius_fraction needs no min-gap partner the way Radial's does.
Vector2 spawn_position(Vector2 center, Vector2 direction, float inner_radius);

// Clamps `inner_radius_fraction` to [0, 1].
float clamp_inner_radius_fraction(float inner);

// Advances fractional spawn `accumulator` by `rate_per_frame` and returns
// how many whole particles that produced this frame, carrying the leftover
// fraction forward so rates below 1/frame still emit correctly across
// multiple frames.
int accumulate_spawn_count(float& accumulator, float rate_per_frame);

// energy/reference normalized to [0, 1], clamped; reference <= 0 yields 0.
// Forced to 0 when idle regardless of energy/reference, so BandAmplitude's
// spawn rate and speed (both derived from this ratio) go to zero rather
// than trickling at some small nonzero value during genuine near-silence.
// Same normalization waveform_math::band_amplitude_scale uses, plus the
// idle gate.
float amplitude_spawn_ratio(float energy, float reference, bool idle);

// Activates the first inactive slot in `pool` with the given
// position/velocity/lifetime. No-op (returns false) if every slot is
// already active.
bool spawn_into_pool(Pool& pool, Vector2 position, Vector2 velocity, int lifetime_frames);

// Advances every active particle in `pool` by one frame (position +=
// velocity, age_frames += 1), deactivating any whose age reaches its
// lifetime. Also deactivates immediately, regardless of remaining lifetime,
// once a particle's position clears the [0, width] x [0, height] canvas by
// more than `margin` on every side -- i.e. once its whole drawn circle
// (radius == margin) has left the visible area, not merely its center point,
// so recycling doesn't clip a still-partially-visible particle. This visual
// has no wraparound/off-canvas-meaningful behavior, so "fully off-canvas" is
// always equivalent to "done" here.
void step_pool(Pool& pool, float width, float height, float margin);

// Alpha-faded draw color for a particle at `age_frames` of
// `lifetime_frames` -- linear fade from base's own alpha at age 0 to fully
// transparent at age == lifetime.
Color faded_color(Color base, int age_frames, int lifetime_frames);

}  // namespace particles_math

struct ParticlesParams {
    Color particle_color = WHITE;
    float radius = 3.0f;
    int lifetime_frames = visual_config::kParticleDefaultLifetimeFrames;
    int burst_count = 24;               // particles spawned per onset event
    float spawn_rate_per_frame = 2.0f;  // particles/frame at ratio*sensitivity == 1 (BandAmplitude)
    float inner_radius_fraction = 0.0f;  // fraction of min(width, height) / 2; 0 = spawn at dead center
};

class ParticlesVisual : public VisualModule {
public:
    explicit ParticlesVisual(ParticlesParams params = {});
    ~ParticlesVisual() override;

    void resize(int width, int height) override;
    void render(const AudioFeatures& features, const LayerReactivity& reactivity) override;
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
    // Not part of params_/clone() -- a clone starts with an empty pool and
    // a fresh accumulator/rng, same as SpectrumVisual's onset_pulse_.
    particles_math::Pool pool_{};
    float spawn_accumulator_ = 0.0f;
    std::mt19937 rng_{std::random_device{}()};
};
