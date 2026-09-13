#include "compositor/layer.h"

#include <stdexcept>

#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/fractal/fractal_visual.h"
#include "visuals/lissajous/lissajous_visual.h"
#include "visuals/particles/particles_visual.h"
#include "visuals/radial/radial_visual.h"
#include "visuals/spectrum/spectrum_visual.h"
#include "visuals/waveform/waveform_visual.h"

std::unique_ptr<VisualModule> make_visual(VisualType type) {
    switch (type) {
        case VisualType::Waveform:
            return std::make_unique<WaveformVisual>();
        case VisualType::Spectrum:
            return std::make_unique<SpectrumVisual>();
        case VisualType::Radial:
            return std::make_unique<RadialVisual>();
        case VisualType::Particles:
            return std::make_unique<ParticlesVisual>();
        case VisualType::Lissajous:
            return std::make_unique<LissajousVisual>();
        case VisualType::FlowField:
            return std::make_unique<FlowFieldVisual>();
        case VisualType::Fractal:
            return std::make_unique<FractalVisual>();
    }
    throw std::invalid_argument("unknown VisualType");
}
