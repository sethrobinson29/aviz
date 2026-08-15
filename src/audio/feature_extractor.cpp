#include "feature_extractor.h"

#include <pffft/pffft.h>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr std::size_t kNumFftBins = audio_config::kFftWindowSize / 2 + 1;  // [0, N/2] inclusive

// dt is the nominal time between process() calls (one hop), used to convert
// an attack/release time constant in ms into a per-call one-pole
// coefficient: coeff = 1 - exp(-dt / tau).
float one_pole_coeff(float time_constant_ms, double dt_seconds) {
    double tau = static_cast<double>(time_constant_ms) / 1000.0;
    if (tau <= 0.0) {
        return 1.0f;  // instantaneous
    }
    return static_cast<float>(1.0 - std::exp(-dt_seconds / tau));
}

std::size_t bin_for_frequency(float hz, unsigned sample_rate) {
    double bin = static_cast<double>(hz) * audio_config::kFftWindowSize / sample_rate;
    std::size_t rounded = static_cast<std::size_t>(std::lround(bin));
    return std::min(rounded, kNumFftBins - 1);
}

}  // namespace

FeatureExtractor::FeatureExtractor(unsigned sample_rate)
    : sample_rate_(sample_rate),
      fft_setup_(pffft_new_setup(static_cast<int>(audio_config::kFftWindowSize), PFFFT_REAL)),
      hann_window_(audio_config::kFftWindowSize),
      windowed_input_(static_cast<float*>(
          pffft_aligned_malloc(audio_config::kFftWindowSize * sizeof(float)))),
      fft_output_(static_cast<float*>(
          pffft_aligned_malloc(audio_config::kFftWindowSize * sizeof(float)))),
      magnitude_(kNumFftBins) {
    for (std::size_t i = 0; i < audio_config::kFftWindowSize; ++i) {
        hann_window_[i] = 0.5f - 0.5f * std::cos(2.0f * kPi * i / (audio_config::kFftWindowSize - 1));
    }

    // Log-scaled band edges from kMinBandFrequencyHz to Nyquist. Each band's
    // bin_lo is the previous band's bin_hi (not independently re-rounded
    // from its own edge_lo) so the bands tile the bin grid contiguously
    // with no gaps or overlap.
    float low = audio_config::kMinBandFrequencyHz;
    float high = static_cast<float>(sample_rate) / 2.0f;
    std::size_t bin_cursor = bin_for_frequency(low, sample_rate);
    for (std::size_t band = 0; band < audio_config::kNumBands; ++band) {
        float edge_hi =
            low * std::pow(high / low, static_cast<float>(band + 1) / audio_config::kNumBands);
        std::size_t bin_hi = std::max(bin_for_frequency(edge_hi, sample_rate), bin_cursor + 1);
        bin_hi = std::min(bin_hi, kNumFftBins);
        assert(bin_hi > bin_cursor && "band collapsed to zero width");
        assert(bin_hi <= kNumFftBins && "band exceeded available FFT bins");
        band_ranges_[band] = {bin_cursor, bin_hi};
        bin_cursor = bin_hi;
    }

    double dt_seconds = static_cast<double>(audio_config::kFftHopSize) / sample_rate_;
    attack_coeff_ = one_pole_coeff(audio_config::kEnvelopeAttackMs, dt_seconds);
    release_coeff_ = one_pole_coeff(audio_config::kEnvelopeReleaseMs, dt_seconds);
    agc_decay_coeff_ = one_pole_coeff(audio_config::kAgcDecayTauMs, dt_seconds);
}

FeatureExtractor::~FeatureExtractor() {
    pffft_aligned_free(windowed_input_);
    pffft_aligned_free(fft_output_);
    pffft_destroy_setup(fft_setup_);
}

void FeatureExtractor::reset_onset_history() {
    prev_energy_.fill(0.0f);
    flux_history_count_.fill(0);
    flux_history_pos_.fill(0);
    for (auto& history : flux_history_) {
        history.fill(0.0f);
    }
    // Deliberately does NOT touch agc_reference_/agc_seeded_.
}

void FeatureExtractor::process(const float* mono_window, bool discontinuity, AudioFeatures& out) {
    if (discontinuity) {
        reset_onset_history();
    }

    std::copy(mono_window, mono_window + audio_config::kFftWindowSize, out.waveform.begin());

    double sum_sq = 0.0;
    for (std::size_t i = 0; i < audio_config::kFftWindowSize; ++i) {
        windowed_input_[i] = mono_window[i] * hann_window_[i];
        sum_sq += static_cast<double>(mono_window[i]) * mono_window[i];
    }
    out.rms = static_cast<float>(std::sqrt(sum_sq / audio_config::kFftWindowSize));

    pffft_transform_ordered(fft_setup_, windowed_input_, fft_output_, nullptr, PFFFT_FORWARD);

    // Ordered real-FFT layout (pffft.h): out[0]=DC.re, out[1]=Nyquist.re,
    // out[2k]/out[2k+1] = re/im of bin k for k in [1, N/2-1].
    magnitude_[0] = std::abs(fft_output_[0]);
    magnitude_[audio_config::kFftWindowSize / 2] = std::abs(fft_output_[1]);
    std::size_t peak_bin = 1;
    float peak_mag = -1.0f;
    for (std::size_t k = 1; k < audio_config::kFftWindowSize / 2; ++k) {
        float re = fft_output_[2 * k];
        float im = fft_output_[2 * k + 1];
        float mag = std::sqrt(re * re + im * im);
        magnitude_[k] = mag;
        if (mag > peak_mag) {
            peak_mag = mag;
            peak_bin = k;
        }
    }
    out.peak_frequency_hz = static_cast<float>(peak_bin) * sample_rate_ / audio_config::kFftWindowSize;

    std::array<float, audio_config::kTotalBandSlots> raw_energy{};
    for (std::size_t band = 0; band < audio_config::kNumBands; ++band) {
        const BandRange& range = band_ranges_[band];
        float sum = 0.0f;
        for (std::size_t bin = range.bin_begin; bin < range.bin_end; ++bin) {
            sum += magnitude_[bin];
        }
        raw_energy[band] = sum / static_cast<float>(range.bin_end - range.bin_begin);
    }
    float global_sum = 0.0f;
    for (std::size_t bin = 0; bin < kNumFftBins; ++bin) {
        global_sum += magnitude_[bin];
    }
    raw_energy[audio_config::kGlobalBandIndex] = global_sum / static_cast<float>(kNumFftBins);

    // Captured once so all bands seed together on the same call.
    bool first_call = !agc_seeded_;

    for (std::size_t band = 0; band < audio_config::kTotalBandSlots; ++band) {
        float flux = std::max(0.0f, raw_energy[band] - prev_energy_[band]);
        prev_energy_[band] = raw_energy[band];

        auto& history = flux_history_[band];
        std::size_t count = flux_history_count_[band];
        std::size_t pos = flux_history_pos_[band];

        float threshold = 0.0f;
        if (count > 0) {
            float mean = 0.0f;
            for (std::size_t i = 0; i < count; ++i) {
                mean += history[i];
            }
            mean /= static_cast<float>(count);
            float variance = 0.0f;
            for (std::size_t i = 0; i < count; ++i) {
                float d = history[i] - mean;
                variance += d * d;
            }
            variance /= static_cast<float>(count);
            float stddev = std::sqrt(variance);
            threshold = mean + audio_config::kOnsetThresholdMultiplier * stddev +
                        audio_config::kOnsetFluxEpsilon;
        }
        // Require a full trailing history before trusting the adaptive
        // threshold -- otherwise it defaults toward 0 and fires on the
        // first nonzero flux regardless of size.
        bool has_baseline = count >= audio_config::kOnsetHistoryLength;
        out.band_onset[band] = has_baseline && flux > 0.0f && flux > threshold;

        history[pos] = flux;
        flux_history_pos_[band] = (pos + 1) % audio_config::kOnsetHistoryLength;
        if (count < audio_config::kOnsetHistoryLength) {
            flux_history_count_[band] = count + 1;
        }

        float coeff = raw_energy[band] > smoothed_energy_[band] ? attack_coeff_ : release_coeff_;
        smoothed_energy_[band] += (raw_energy[band] - smoothed_energy_[band]) * coeff;
        out.band_energy[band] = smoothed_energy_[band];

        // Peak-hold-with-slow-release, tracking smoothed_energy_ (not
        // raw_energy -- see audio_config.h). Rise is an exact snap, not a
        // blend: any new peak is captured immediately, with no catch-up lag
        // to tune away.
        if (first_call || smoothed_energy_[band] > agc_reference_[band]) {
            agc_reference_[band] = smoothed_energy_[band];
        } else {
            agc_reference_[band] += (smoothed_energy_[band] - agc_reference_[band]) * agc_decay_coeff_;
        }
        agc_reference_[band] = std::max(agc_reference_[band], audio_config::kAgcMinReference);
        out.band_agc_reference[band] = agc_reference_[band];
    }

    agc_seeded_ = true;
}
