#include "compositor/visual_param.h"

#include <variant>

#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/fractal/fractal_visual.h"
#include "visuals/lissajous/lissajous_visual.h"
#include "visuals/particles/particles_visual.h"
#include "visuals/radial/radial_visual.h"

namespace {

bool set_particles_param(ParticlesVisual& visual, const std::string& param_name, const VisualParamValue& value) {
    const float* number = std::get_if<float>(&value);
    if (number == nullptr) {
        return false;
    }
    if (param_name == "spawn_rate_per_frame") {
        visual.mutable_params().spawn_rate_per_frame = particles_math::clamp_spawn_rate_per_frame(*number);
        return true;
    }
    if (param_name == "lifetime_frames") {
        visual.mutable_params().lifetime_frames = particles_math::clamp_lifetime_frames_from_number(*number);
        return true;
    }
    if (param_name == "inner_radius_fraction") {
        visual.mutable_params().inner_radius_fraction = particles_math::clamp_inner_radius_fraction(*number);
        return true;
    }
    return false;
}

bool set_flow_field_param(FlowFieldVisual& visual, const std::string& param_name, const VisualParamValue& value) {
    const float* number = std::get_if<float>(&value);
    const bool* flag = std::get_if<bool>(&value);
    if (param_name == "spawn_rate_per_frame" && number != nullptr) {
        visual.mutable_params().spawn_rate_per_frame = flow_field_math::clamp_spawn_rate_per_frame(*number);
        return true;
    }
    if (param_name == "lifetime_frames" && number != nullptr) {
        visual.mutable_params().lifetime_frames = particles_math::clamp_lifetime_frames_from_number(*number);
        return true;
    }
    if (param_name == "turbulence_frequency" && number != nullptr) {
        visual.mutable_params().turbulence_frequency = flow_field_math::clamp_turbulence_frequency(*number);
        return true;
    }
    if (param_name == "evolution_per_frame" && number != nullptr) {
        visual.mutable_params().evolution_per_frame = flow_field_math::clamp_evolution_per_frame(*number);
        return true;
    }
    if (param_name == "grid_cols" && number != nullptr) {
        visual.mutable_params().grid_cols = flow_field_math::clamp_grid_cols_from_number(*number);
        return true;
    }
    if (param_name == "ripple_on_onset_enabled" && flag != nullptr) {
        visual.mutable_params().ripple_on_onset_enabled = *flag;
        return true;
    }
    if (param_name == "burst_on_onset_enabled" && flag != nullptr) {
        visual.mutable_params().burst_on_onset_enabled = *flag;
        return true;
    }
    return false;
}

bool set_radial_param(RadialVisual& visual, const std::string& param_name, const VisualParamValue& value) {
    const float* number = std::get_if<float>(&value);
    if (number == nullptr) {
        return false;
    }
    RadialParams& params = visual.mutable_params();
    if (param_name == "inner_radius_fraction") {
        params.inner_radius_fraction = radial_math::clamp_inner_radius_fraction(*number, params.max_radius_fraction);
        return true;
    }
    if (param_name == "max_radius_fraction") {
        params.max_radius_fraction = radial_math::clamp_max_radius_fraction(*number, params.inner_radius_fraction);
        return true;
    }
    return false;
}

bool set_lissajous_param(LissajousVisual& visual, const std::string& param_name, const VisualParamValue& value) {
    const float* number = std::get_if<float>(&value);
    if (param_name == "compression_exponent" && number != nullptr) {
        visual.mutable_params().compression_exponent = lissajous_math::clamp_compression_exponent(*number);
        return true;
    }
    return false;
}

bool set_fractal_param(FractalVisual& visual, const std::string& param_name, const VisualParamValue& value) {
    const float* number = std::get_if<float>(&value);
    const bool* flag = std::get_if<bool>(&value);
    FractalParams& params = visual.mutable_params();
    if (param_name == "julia_orbit_clockwise" && flag != nullptr) {
        params.julia_orbit_clockwise = *flag;
        return true;
    }
    if (number == nullptr) {
        return false;
    }
    if (param_name == "base_zoom") {
        params.base_zoom = fractal_math::clamp_base_zoom(*number);
        return true;
    }
    if (param_name == "zoom_amplitude_scale") {
        params.zoom_amplitude_scale = fractal_math::clamp_zoom_amplitude_scale(*number);
        return true;
    }
    if (param_name == "julia_base_radius") {
        params.julia_base_radius = fractal_math::clamp_julia_base_radius(*number);
        return true;
    }
    if (param_name == "julia_radius_amplitude_scale") {
        params.julia_radius_amplitude_scale = fractal_math::clamp_julia_radius_amplitude_scale(*number);
        return true;
    }
    if (param_name == "julia_angular_speed_radians_per_sec") {
        params.julia_angular_speed_radians_per_sec = fractal_math::clamp_julia_angular_speed(*number);
        return true;
    }
    if (param_name == "julia_orbit_center_x") {
        params.julia_orbit_center_x = fractal_math::clamp_julia_orbit_center(*number);
        return true;
    }
    if (param_name == "julia_orbit_center_y") {
        params.julia_orbit_center_y = fractal_math::clamp_julia_orbit_center(*number);
        return true;
    }
    if (param_name == "color_cycle_speed_hz") {
        params.color_cycle_speed_hz = fractal_math::clamp_color_cycle_speed_hz(*number);
        return true;
    }
    return false;
}

}

namespace visual_param {

bool set(Layer& layer, const std::string& param_name, const VisualParamValue& value) {
    switch (layer.visual_type) {
        case VisualType::Particles:
            return set_particles_param(dynamic_cast<ParticlesVisual&>(*layer.visual), param_name, value);
        case VisualType::FlowField:
            return set_flow_field_param(dynamic_cast<FlowFieldVisual&>(*layer.visual), param_name, value);
        case VisualType::Radial:
            return set_radial_param(dynamic_cast<RadialVisual&>(*layer.visual), param_name, value);
        case VisualType::Lissajous:
            return set_lissajous_param(dynamic_cast<LissajousVisual&>(*layer.visual), param_name, value);
        case VisualType::Fractal:
            return set_fractal_param(dynamic_cast<FractalVisual&>(*layer.visual), param_name, value);
        default:
            return false;
    }
}

}
