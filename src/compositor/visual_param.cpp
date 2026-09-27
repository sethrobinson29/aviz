#include "compositor/visual_param.h"

#include "visuals/particles/particles_visual.h"

namespace {

bool set_particles_param(ParticlesVisual& visual, const std::string& param_name, float value) {
    if (param_name == "spawn_rate_per_frame") {
        visual.mutable_params().spawn_rate_per_frame = particles_math::clamp_spawn_rate_per_frame(value);
        return true;
    }
    if (param_name == "lifetime_frames") {
        visual.mutable_params().lifetime_frames =
            particles_math::clamp_lifetime_frames(static_cast<int>(value));
        return true;
    }
    return false;
}

}

namespace visual_param {

bool set(Layer& layer, const std::string& param_name, float value) {
    switch (layer.visual_type) {
        case VisualType::Particles:
            return set_particles_param(dynamic_cast<ParticlesVisual&>(*layer.visual), param_name, value);
        default:
            return false;
    }
}

}
