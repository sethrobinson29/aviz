#include "compositor/compositor_math.h"

#include "raylib.h"
#include "rlgl.h"

namespace compositor_math {

float clamp_opacity(float opacity) { return std::clamp(opacity, 0.0f, 1.0f); }

BlendFactors to_blend_factors(LayerBlendMode mode) {
    switch (mode) {
        case LayerBlendMode::Normal:
            return BlendFactors{BLEND_ALPHA, 0, 0, 0};
        case LayerBlendMode::Additive:
            return BlendFactors{BLEND_ADDITIVE, 0, 0, 0};
        case LayerBlendMode::Multiply:
            return BlendFactors{BLEND_MULTIPLIED, 0, 0, 0};
        case LayerBlendMode::Screen:
            // result = src + dst - src*dst -- classic "screen" formula.
            return BlendFactors{BLEND_CUSTOM, RL_ONE_MINUS_DST_COLOR, RL_ONE, RL_FUNC_ADD};
    }
    return BlendFactors{BLEND_ALPHA, 0, 0, 0};
}

}  // namespace compositor_math
