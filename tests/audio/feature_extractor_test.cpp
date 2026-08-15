#include "audio/feature_extractor.h"

#include <cmath>

#include <gtest/gtest.h>

#include "fixtures/synthetic_signal.h"

namespace {

constexpr unsigned kSampleRate = 48000;

// Independent re-derivation of the log-band edge formula from spec §4 /
// audio_config.h's public constants (kMinBandFrequencyHz, kNumBands) — not
// a call into FeatureExtractor's private banding, so this actually checks
// the extractor's output against the documented band layout rather than
// against itself.
std::size_t band_for_frequency(float hz, unsigned sample_rate) {
    float low = audio_config::kMinBandFrequencyHz;
    float high = static_cast<float>(sample_rate) / 2.0f;
    for (std::size_t b = 0; b < audio_config::kNumBands; ++b) {
        float edge_hi = low * std::pow(high / low, static_cast<float>(b + 1) / audio_config::kNumBands);
        if (hz < edge_hi) {
            return b;
        }
    }
    return audio_config::kNumBands - 1;
}

}  // namespace

// --- Banding -------------------------------------------------------------

TEST(FeatureExtractorTest, KnownFrequencyLandsInExpectedBandAndPeak) {
    constexpr float kToneHz = 1000.0f;
    auto tone = fixtures::sine_wave(kToneHz, kSampleRate, audio_config::kFftWindowSize);

    FeatureExtractor extractor(kSampleRate);
    AudioFeatures out;
    // Let the envelope settle toward the tone's steady-state energy.
    for (int i = 0; i < 50; ++i) {
        extractor.process(tone.data(), false, out);
    }

    EXPECT_NEAR(out.peak_frequency_hz, kToneHz, 100.0f);

    std::size_t expected_band = band_for_frequency(kToneHz, kSampleRate);
    std::size_t max_band = 0;
    for (std::size_t b = 1; b < audio_config::kNumBands; ++b) {
        if (out.band_energy[b] > out.band_energy[max_band]) {
            max_band = b;
        }
    }
    EXPECT_EQ(max_band, expected_band);
}

// --- Onset differentiation ------------------------------------------------
//
// TESTING.md: "a synthetic kick-only impulse train and a synthetic hi-hat-
// only impulse train to validate that per-band onset detection actually
// behaves differently per band."

namespace {

// Runs `signal` through a fresh extractor in overlapping hop-spaced windows
// and returns, per band, whether that band ever fired an onset.
std::array<bool, audio_config::kTotalBandSlots> onsets_seen(const std::vector<float>& signal) {
    FeatureExtractor extractor(kSampleRate);
    std::array<bool, audio_config::kTotalBandSlots> seen{};
    AudioFeatures out;
    for (std::size_t pos = 0; pos + audio_config::kFftWindowSize <= signal.size();
         pos += audio_config::kFftHopSize) {
        extractor.process(signal.data() + pos, false, out);
        for (std::size_t b = 0; b < audio_config::kTotalBandSlots; ++b) {
            seen[b] = seen[b] || out.band_onset[b];
        }
    }
    return seen;
}

// A hard rectangular on/off gate is itself a broadband click (spectral
// leakage into every band) independent of the carrier frequency, which
// would swamp the per-band distinction this test is checking. Softening the
// burst edges with a short raised-cosine ramp keeps it percussive (still
// well within a single FFT hop) while avoiding that self-inflicted
// broadband artifact — closer to a real drum hit's envelope than an
// infinitely sharp digital edge.
std::vector<float> gated_tone(float carrier_hz, std::size_t num_samples, std::size_t period,
                               std::size_t burst_len) {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr std::size_t kRampLen = 64;

    auto carrier = fixtures::sine_wave(carrier_hz, kSampleRate, num_samples);
    auto gate = fixtures::impulse_train(num_samples, period, burst_len);
    for (std::size_t start = 0; start < num_samples; start += period) {
        for (std::size_t i = 0; i < kRampLen && start + i < num_samples; ++i) {
            float w = 0.5f - 0.5f * std::cos(kPi * static_cast<float>(i) / kRampLen);
            gate[start + i] *= w;
        }
        if (burst_len >= kRampLen) {
            std::size_t fade_start = start + burst_len - kRampLen;
            for (std::size_t i = 0; i < kRampLen && fade_start + i < num_samples; ++i) {
                float w = 0.5f + 0.5f * std::cos(kPi * static_cast<float>(i) / kRampLen);
                gate[fade_start + i] *= w;
            }
        }
    }
    for (std::size_t i = 0; i < num_samples; ++i) {
        carrier[i] *= gate[i];
    }
    return carrier;
}

}  // namespace

TEST(FeatureExtractorTest, KickAndHiHatFireDistinctBands) {
    constexpr float kKickHz = 60.0f;
    constexpr float kHiHatHz = 8000.0f;
    constexpr std::size_t kNumSamples = kSampleRate * 2;   // 2 seconds
    constexpr std::size_t kPeriod = kSampleRate / 2;       // 2 beats/sec
    constexpr std::size_t kBurstLen = 2048;                // wider than one FFT window

    std::size_t kick_band = band_for_frequency(kKickHz, kSampleRate);
    std::size_t hihat_band = band_for_frequency(kHiHatHz, kSampleRate);
    ASSERT_NE(kick_band, hihat_band);

    auto kick_signal = gated_tone(kKickHz, kNumSamples, kPeriod, kBurstLen);
    auto kick_onsets = onsets_seen(kick_signal);
    EXPECT_TRUE(kick_onsets[kick_band]) << "kick band never fired an onset";
    EXPECT_FALSE(kick_onsets[hihat_band]) << "hi-hat band fired on a kick-only signal";

    auto hihat_signal = gated_tone(kHiHatHz, kNumSamples, kPeriod, kBurstLen);
    auto hihat_onsets = onsets_seen(hihat_signal);
    EXPECT_TRUE(hihat_onsets[hihat_band]) << "hi-hat band never fired an onset";
    EXPECT_FALSE(hihat_onsets[kick_band]) << "kick band fired on a hi-hat-only signal";
}

// --- Envelope attack/release ----------------------------------------------

TEST(FeatureExtractorTest, AttackRisesFasterThanRelease) {
    constexpr float kToneHz = 1000.0f;
    auto tone = fixtures::sine_wave(kToneHz, kSampleRate, audio_config::kFftWindowSize);
    auto quiet = fixtures::silence(audio_config::kFftWindowSize);

    FeatureExtractor extractor(kSampleRate);
    std::size_t band = band_for_frequency(kToneHz, kSampleRate);
    AudioFeatures out;

    // Let the envelope fully settle at the tone's steady-state energy.
    for (int i = 0; i < 200; ++i) {
        extractor.process(tone.data(), false, out);
    }
    float steady_state = out.band_energy[band];
    ASSERT_GT(steady_state, 0.0f);

    // Attack: from a fresh (zeroed) extractor, count calls to cross halfway.
    FeatureExtractor rising(kSampleRate);
    AudioFeatures rising_out;
    int attack_calls_to_half = -1;
    for (int i = 1; i <= 200; ++i) {
        rising.process(tone.data(), false, rising_out);
        if (attack_calls_to_half < 0 && rising_out.band_energy[band] >= steady_state / 2.0f) {
            attack_calls_to_half = i;
            break;
        }
    }
    ASSERT_GT(attack_calls_to_half, 0) << "energy never reached halfway during attack";

    // Release: starting from the settled `extractor` above, count calls to
    // fall back below halfway once the tone stops.
    int release_calls_to_half = -1;
    for (int i = 1; i <= 400; ++i) {
        extractor.process(quiet.data(), false, out);
        if (release_calls_to_half < 0 && out.band_energy[band] <= steady_state / 2.0f) {
            release_calls_to_half = i;
            break;
        }
    }
    ASSERT_GT(release_calls_to_half, 0) << "energy never fell back below halfway during release";

    // kEnvelopeReleaseMs (200ms) is 20x kEnvelopeAttackMs (10ms), so release
    // should take noticeably more calls to cover the same half-distance.
    EXPECT_GT(release_calls_to_half, attack_calls_to_half);
}
