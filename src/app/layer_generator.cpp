#include "app/layer_generator.h"

#include <algorithm>
#include <numeric>

#include "audio/audio_config.h"

namespace {

ReactivitySource random_band_reactivity(std::mt19937& rng) {
    std::uniform_int_distribution<int> dist(0, 1);
    return dist(rng) == 0 ? ReactivitySource::BandOnset : ReactivitySource::BandAmplitude;
}

}

std::vector<GeneratedLayerSpec> generate_layer_specs(std::size_t count, std::mt19937& rng) {
    std::vector<GeneratedLayerSpec> specs(count);
    if (count == 0) {
        return specs;
    }

    std::uniform_int_distribution<int> visual_dist(0, 4);
    std::uniform_int_distribution<std::size_t> band_choice_dist(0, audio_config::kNumBands);
    for (GeneratedLayerSpec& spec : specs) {
        spec.visual_type = static_cast<VisualType>(visual_dist(rng));
        std::size_t choice = band_choice_dist(rng);
        if (choice == audio_config::kNumBands) {
            spec.freq_band = std::nullopt;
            spec.reactivity_source = ReactivitySource::GlobalBeat;
        } else {
            spec.freq_band = choice;
            spec.reactivity_source = random_band_reactivity(rng);
        }
    }

    std::vector<std::size_t> indices(count);
    std::iota(indices.begin(), indices.end(), 0);
    std::vector<bool> band_forced(count, false);

    if (count > audio_config::kNumBands) {
        std::vector<std::size_t> shuffled_indices = indices;
        std::shuffle(shuffled_indices.begin(), shuffled_indices.end(), rng);

        std::vector<std::size_t> bands(audio_config::kNumBands);
        std::iota(bands.begin(), bands.end(), 0);
        std::shuffle(bands.begin(), bands.end(), rng);

        for (std::size_t i = 0; i < audio_config::kNumBands; ++i) {
            std::size_t idx = shuffled_indices[i];
            specs[idx].freq_band = bands[i];
            specs[idx].reactivity_source = random_band_reactivity(rng);
            band_forced[idx] = true;
        }
    }

    {
        const VisualType required_visuals[] = {VisualType::Waveform, VisualType::Spectrum,
                                                 VisualType::Radial};
        std::vector<std::size_t> shuffled_indices = indices;
        std::shuffle(shuffled_indices.begin(), shuffled_indices.end(), rng);
        std::size_t n = std::min(count, std::size(required_visuals));
        for (std::size_t i = 0; i < n; ++i) {
            specs[shuffled_indices[i]].visual_type = required_visuals[i];
        }
    }

    {
        std::vector<std::size_t> filler_indices;
        std::vector<std::size_t> forced_band_indices;
        for (std::size_t idx : indices) {
            (band_forced[idx] ? forced_band_indices : filler_indices).push_back(idx);
        }
        std::shuffle(filler_indices.begin(), filler_indices.end(), rng);
        std::shuffle(forced_band_indices.begin(), forced_band_indices.end(), rng);

        std::vector<std::size_t> fallback_indices = indices;
        std::shuffle(fallback_indices.begin(), fallback_indices.end(), rng);
        std::size_t fallback_cursor = 0;
        auto next_fallback = [&]() -> std::size_t {
            std::size_t idx = fallback_indices[fallback_cursor % fallback_indices.size()];
            ++fallback_cursor;
            return idx;
        };

        std::size_t global_idx;
        if (!filler_indices.empty()) {
            global_idx = filler_indices.back();
            filler_indices.pop_back();
        } else {
            global_idx = next_fallback();
        }
        specs[global_idx].freq_band = std::nullopt;
        specs[global_idx].reactivity_source = ReactivitySource::GlobalBeat;

        std::size_t onset_idx;
        if (!forced_band_indices.empty()) {
            onset_idx = forced_band_indices.back();
            forced_band_indices.pop_back();
        } else {
            onset_idx = next_fallback();
            specs[onset_idx].freq_band = onset_idx % audio_config::kNumBands;
        }
        specs[onset_idx].reactivity_source = ReactivitySource::BandOnset;

        std::size_t amplitude_idx;
        if (!forced_band_indices.empty()) {
            amplitude_idx = forced_band_indices.back();
            forced_band_indices.pop_back();
        } else {
            amplitude_idx = next_fallback();
            specs[amplitude_idx].freq_band = amplitude_idx % audio_config::kNumBands;
        }
        specs[amplitude_idx].reactivity_source = ReactivitySource::BandAmplitude;
    }

    return specs;
}
