#pragma once

#include "audio/audio_features.h"
#include "raylib.h"

class VisualModule {
public:
    virtual ~VisualModule() = default;

    // Requires a live raylib window/GL context.
    virtual void resize(int width, int height) = 0;

    // Draws one frame into the offscreen texture from the current features.
    virtual void render(const AudioFeatures& features) = 0;

    virtual RenderTexture2D& render_texture() = 0;
};
