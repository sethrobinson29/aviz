#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "audio_config.h"
#include "audio_features.h"

struct PFFFT_Setup;

// Turns one time-domain analysis window into one AudioFeatures snapshot:
// windowed real FFT (pffft), log-band energy mapping, per-band spectral-flux
// onset detection, and one-pole attack/release envelope smoothing (spec §4).
// Pure logic, no threading/IO — unit-testable directly against synthetic
// fixtures per TESTING.md. Not thread-safe: the feature worker owns one
// instance and calls process() sequentially from a single thread.
class FeatureExtractor {
public:
    explicit FeatureExtractor(unsigned sample_rate);
    ~FeatureExtractor();

    FeatureExtractor(const FeatureExtractor&) = delete;
    FeatureExtractor& operator=(const FeatureExtractor&) = delete;

    // `mono_window` must have exactly audio_config::kFftWindowSize samples.
    // `discontinuity` resets per-band onset history (flux comparisons across
    // a ring-buffer overwrite gap would otherwise look like a spurious
    // transient, spec §3) but leaves envelope state alone — the envelope has
    // no notion of temporal contiguity to violate, it just tracks a value.
    void process(const float* mono_window, bool discontinuity, AudioFeatures& out);

private:
    struct BandRange {
        std::size_t bin_begin;
        std::size_t bin_end;  // exclusive
    };

    unsigned sample_rate_;
    PFFFT_Setup* fft_setup_;
    std::vector<float> hann_window_;

    // pffft requires 16-byte-aligned buffers; std::vector's alignment
    // guarantee isn't sufficient by the library's own documented contract,
    // so these are allocated via pffft_aligned_malloc/free instead.
    float* windowed_input_;  // pffft input, size kFftWindowSize
    float* fft_output_;      // pffft ordered output, size kFftWindowSize

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

    void reset_onset_history();
};
