#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "audio/audio_features.h"
#include "raylib.h"

enum class ReactivitySource { GlobalBeat, BandOnset, BandAmplitude };

struct LayerReactivity {
    std::optional<std::size_t> freq_band;
    ReactivitySource source = ReactivitySource::GlobalBeat;
    float sensitivity = 1.0f;
};

class VisualModule {
public:
    virtual ~VisualModule() = default;

    virtual void resize(int width, int height) = 0;

    virtual void render(const AudioFeatures& features, const LayerReactivity& reactivity) = 0;

    virtual RenderTexture2D& render_texture() = 0;

    virtual std::unique_ptr<VisualModule> clone() const = 0;
};
