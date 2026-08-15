#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "audio_config.h"
#include "audio_features.h"

struct PFFFT_Setup;

// Turns one time-domain analysis window into one AudioFeatures snapshot.
// Not thread-safe: the feature worker owns one instance and calls process()
// sequentially from a single thread.
class FeatureExtractor {
public:
    explicit FeatureExtractor(unsigned sample_rate);
    ~FeatureExtractor();

    FeatureExtractor(const FeatureExtractor&) = delete;
    FeatureExtractor& operator=(const FeatureExtractor&) = delete;

    // `mono_window` must have exactly audio_config::kFftWindowSize samples.
    // `discontinuity` resets per-band onset history; envelope/AGC state is
    // left alone.
    void process(const float* mono_window, bool discontinuity, AudioFeatures& out);

private:
    struct BandRange {
        std::size_t bin_begin;
        std::size_t bin_end;  // exclusive
    };

    unsigned sample_rate_;
    PFFFT_Setup* fft_setup_;
    std::vector<float> hann_window_;

    // Allocated via pffft_aligned_malloc/free -- pffft requires 16-byte
    // alignment, which std::vector doesn't guarantee.
    float* windowed_input_;  // size kFftWindowSize
    float* fft_output_;      // ordered output, size kFftWindowSize

    std::vector<float> magnitude_;  // bins [0, kFftWindowSize/2]
    std::array<BandRange, audio_config::kNumBands> band_ranges_{};

    std::array<float, audio_config::kTotalBandSlots> prev_energy_{};
    std::array<float, audio_config::kTotalBandSlots> smoothed_energy_{};
    std::array<std::array<float, audio_config::kOnsetHistoryLength>, audio_config::kTotalBandSlots>
        flux_history_{};
    std::array<std::size_t, audio_config::kTotalBandSlots> flux_history_count_{};
    std::array<std::size_t, audio_config::kTotalBandSlots> flux_history_pos_{};

    float attack_coeff_;
    float release_coeff_;

    // Peak-hold-with-slow-release reference tracking smoothed_energy_: an
    // exact snap on rise, a slow decay (audio_config::kAgcDecayTauMs)
    // otherwise. Seeded on the first process() call; agc_seeded_ never
    // resets, not even on `discontinuity`.
    std::array<float, audio_config::kTotalBandSlots> agc_reference_{};
    float agc_decay_coeff_;
    bool agc_seeded_ = false;

    void reset_onset_history();
};
