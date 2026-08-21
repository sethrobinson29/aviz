#include "compositor/layer.h"

#include <stdexcept>

#include "visuals/spectrum/spectrum_visual.h"
#include "visuals/waveform/waveform_visual.h"

std::unique_ptr<VisualModule> make_visual(VisualType type) {
    switch (type) {
        case VisualType::Waveform:
            return std::make_unique<WaveformVisual>();
        case VisualType::Spectrum:
            return std::make_unique<SpectrumVisual>();
    }
    throw std::invalid_argument("unknown VisualType");
}
