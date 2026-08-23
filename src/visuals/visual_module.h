#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "audio/audio_features.h"
#include "raylib.h"

// What audio-derived value a layer's visual is driven by. band_onset and
// band_amplitude only make sense with freq_band set; global_beat and
// peak_freq only make sense with freq_band == nullopt (Global).
enum class ReactivitySource { GlobalBeat, BandOnset, BandAmplitude, PeakFreq };

// A layer's reactivity scoping, passed into VisualModule::render() so a
// visual can filter/modulate its display without touching Layer directly
// (avoids a visuals/ <-> compositor/ include cycle).
struct LayerReactivity {
    // nullopt = Global; otherwise a band index in [0, audio_config::kNumBands).
    std::optional<std::size_t> freq_band;
    ReactivitySource source = ReactivitySource::GlobalBeat;
    // Mirrors Layer::sensitivity; see compositor_math::clamp_sensitivity.
    float sensitivity = 1.0f;
};

class VisualModule {
public:
    virtual ~VisualModule() = default;

    // Requires a live raylib window/GL context.
    virtual void resize(int width, int height) = 0;

    // Draws one frame into the offscreen texture from the current features,
    // filtered/modulated per `reactivity` if the concrete visual honors it.
    virtual void render(const AudioFeatures& features, const LayerReactivity& reactivity) = 0;

    virtual RenderTexture2D& render_texture() = 0;

    // Returns a new, independent instance of the same concrete type with
    // the same visual_params. Does not copy render_texture() state -- the
    // clone starts unresized like any freshly constructed VisualModule.
    virtual std::unique_ptr<VisualModule> clone() const = 0;
};
