#include "compositor/layer.h"

#include <stdexcept>

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
    }
    throw std::invalid_argument("unknown VisualType");
}
