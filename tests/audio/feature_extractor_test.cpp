#include "audio/feature_extractor.h"

#include <cmath>

#include <gtest/gtest.h>

#include "fixtures/synthetic_signal.h"

namespace {

constexpr unsigned kSampleRate = 48000;

std::size_t band_for_frequency(float hz, unsigned sample_rate) {
    constexpr std::size_t kNumFftBins = audio_config::kFftWindowSize / 2 + 1;
    float low = audio_config::kMinBandFrequencyHz;
    float high = static_cast<float>(sample_rate) / 2.0f;

    auto bin_for = [sample_rate, kNumFftBins](float f) -> std::size_t {
        double bin = static_cast<double>(f) * audio_config::kFftWindowSize / sample_rate;
        std::size_t rounded = static_cast<std::size_t>(std::lround(bin));
        return std::min(rounded, kNumFftBins - 1);
    };

    std::size_t target_bin = bin_for(hz);
    std::size_t bin_cursor = bin_for(low);
    for (std::size_t b = 0; b < audio_config::kNumBands; ++b) {
        float edge_hi = low * std::pow(high / low, static_cast<float>(b + 1) / audio_config::kNumBands);
        std::size_t bin_hi = std::max(bin_for(edge_hi), bin_cursor + 1);
        bin_hi = std::min(bin_hi, kNumFftBins);
        if (target_bin < bin_hi) {
            return b;
        }
        bin_cursor = bin_hi;
    }
    return audio_config::kNumBands - 1;
}

}

TEST(FeatureExtractorTest, KnownFrequencyLandsInExpectedBandAndPeak) {
    constexpr float kToneHz = 1000.0f;
    auto tone = fixtures::sine_wave(kToneHz, kSampleRate, audio_config::kFftWindowSize);

    FeatureExtractor extractor(kSampleRate);
    AudioFeatures out;
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

TEST(FeatureExtractorTest, LowFrequencyBandsDoNotOverlap) {
    constexpr float kBin1Hz = 46.875f;
    constexpr float kBin2Hz = 93.75f;

    auto tone1 = fixtures::sine_wave(kBin1Hz, kSampleRate, audio_config::kFftWindowSize);
    auto tone2 = fixtures::sine_wave(kBin2Hz, kSampleRate, audio_config::kFftWindowSize);

    FeatureExtractor extractor1(kSampleRate);
    FeatureExtractor extractor2(kSampleRate);
    AudioFeatures out1;
    AudioFeatures out2;
    for (int i = 0; i < 50; ++i) {
        extractor1.process(tone1.data(), false, out1);
        extractor2.process(tone2.data(), false, out2);
    }

    std::size_t band1 = 0;
    std::size_t band2 = 0;
    for (std::size_t b = 1; b < audio_config::kNumBands; ++b) {
        if (out1.band_energy[b] > out1.band_energy[band1]) band1 = b;
        if (out2.band_energy[b] > out2.band_energy[band2]) band2 = b;
    }

    EXPECT_EQ(band1, band_for_frequency(kBin1Hz, kSampleRate));
    EXPECT_EQ(band2, band_for_frequency(kBin2Hz, kSampleRate));
    EXPECT_NE(band1, band2) << "adjacent low-frequency bins mapped to the same band";
}

namespace {

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

}

TEST(FeatureExtractorTest, KickAndHiHatFireDistinctBands) {
    constexpr float kKickHz = 60.0f;
    constexpr float kHiHatHz = 8000.0f;
    constexpr std::size_t kNumSamples = kSampleRate * 2;
    constexpr std::size_t kPeriod = kSampleRate / 2;
    constexpr std::size_t kBurstLen = 2048;

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

TEST(FeatureExtractorTest, AttackRisesFasterThanRelease) {
    constexpr float kToneHz = 1000.0f;
    auto tone = fixtures::sine_wave(kToneHz, kSampleRate, audio_config::kFftWindowSize);
    auto quiet = fixtures::silence(audio_config::kFftWindowSize);

    FeatureExtractor extractor(kSampleRate);
    std::size_t band = band_for_frequency(kToneHz, kSampleRate);
    AudioFeatures out;

    for (int i = 0; i < 200; ++i) {
        extractor.process(tone.data(), false, out);
    }
    float steady_state = out.band_energy[band];
    ASSERT_GT(steady_state, 0.0f);

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

    int release_calls_to_half = -1;
    for (int i = 1; i <= 400; ++i) {
        extractor.process(quiet.data(), false, out);
        if (release_calls_to_half < 0 && out.band_energy[band] <= steady_state / 2.0f) {
            release_calls_to_half = i;
            break;
        }
    }
    ASSERT_GT(release_calls_to_half, 0) << "energy never fell back below halfway during release";

    EXPECT_GT(release_calls_to_half, attack_calls_to_half);
}

TEST(FeatureExtractorTest, IdleFloorMarksBandIdleWithoutForcingSmoothedEnergyToZero) {
    constexpr float kToneHz = 1000.0f;
    auto tone = fixtures::sine_wave(kToneHz, kSampleRate, audio_config::kFftWindowSize);
    auto quiet = fixtures::silence(audio_config::kFftWindowSize);
    std::size_t band = band_for_frequency(kToneHz, kSampleRate);

    FeatureExtractor extractor(kSampleRate);
    AudioFeatures out;
    for (int i = 0; i < 200; ++i) {
        extractor.process(tone.data(), false, out);
    }
    ASSERT_GT(out.band_energy[band], 0.0f);
    ASSERT_FALSE(out.band_idle[band]);

    bool saw_idle = false;
    float prev_energy = out.band_energy[band];
    for (int i = 0; i < 100 && !saw_idle; ++i) {
        extractor.process(quiet.data(), false, out);
        if (out.band_idle[band]) {
            saw_idle = true;
            EXPECT_GT(out.band_energy[band], prev_energy * 0.5f);
            EXPECT_GT(out.band_energy[band], 0.0f);
        }
        prev_energy = out.band_energy[band];
    }
    ASSERT_TRUE(saw_idle);

    for (int i = 0; i < 400; ++i) {
        extractor.process(quiet.data(), false, out);
    }
    EXPECT_GT(out.band_energy[band], 0.0f);
}

TEST(FeatureExtractorTest, IdleFloorScalesWithBandsOwnReferenceNotAbsoluteMagnitude) {
    constexpr float kToneHz = 1000.0f;
    constexpr float kQuietAmplitude = 0.0001f;
    auto quiet_tone = fixtures::sine_wave(kToneHz, kSampleRate, audio_config::kFftWindowSize, kQuietAmplitude);
    std::size_t band = band_for_frequency(kToneHz, kSampleRate);

    FeatureExtractor extractor(kSampleRate);
    AudioFeatures out;
    for (int i = 0; i < 200; ++i) {
        extractor.process(quiet_tone.data(), false, out);
    }

    EXPECT_GT(out.band_energy[band], 0.0f);
    EXPECT_GT(out.band_energy[band] / out.band_agc_reference[band], 0.5f)
        << "band_energy=" << out.band_energy[band] << " reference=" << out.band_agc_reference[band];
    EXPECT_FALSE(out.band_idle[band]);
}

TEST(FeatureExtractorTest, IdleCheckReadsThisHopsUpdatedReferenceNotStalePreviousValue) {
    auto silence = fixtures::silence(audio_config::kFftWindowSize);
    FeatureExtractor extractor(kSampleRate);
    AudioFeatures out;

    extractor.process(silence.data(), false, out);

    for (std::size_t b = 0; b < audio_config::kTotalBandSlots; ++b) {
        EXPECT_TRUE(out.band_idle[b]) << "band " << b;
    }
}

TEST(FeatureExtractorTest, IdleFloorAppliesPerBandIndependently) {
    constexpr float kSteadyHz = 8000.0f;
    constexpr float kSilentHz = 60.0f;
    auto steady_tone = fixtures::sine_wave(kSteadyHz, kSampleRate, audio_config::kFftWindowSize);
    std::size_t steady_band = band_for_frequency(kSteadyHz, kSampleRate);
    std::size_t silent_band = band_for_frequency(kSilentHz, kSampleRate);
    ASSERT_NE(steady_band, silent_band);

    FeatureExtractor extractor(kSampleRate);
    AudioFeatures out;
    for (int i = 0; i < 200; ++i) {
        extractor.process(steady_tone.data(), false, out);
    }

    EXPECT_GT(out.band_energy[steady_band], 0.0f)
        << "band with real, stable energy must not be forced idle by another band's state";
    EXPECT_FALSE(out.band_idle[steady_band]);

    float steady_ratio = out.band_energy[steady_band] / out.band_agc_reference[steady_band];
    float silent_ratio = out.band_energy[silent_band] / out.band_agc_reference[silent_band];
    EXPECT_LT(silent_ratio, steady_ratio);
}

TEST(FeatureExtractorTest, AgcReferenceSeededFromFirstCallNotZero) {
    constexpr float kToneHz = 1000.0f;
    auto tone = fixtures::sine_wave(kToneHz, kSampleRate, audio_config::kFftWindowSize);

    FeatureExtractor extractor(kSampleRate);
    std::size_t band = band_for_frequency(kToneHz, kSampleRate);
    AudioFeatures out;

    extractor.process(tone.data(), false, out);

    EXPECT_GT(out.band_agc_reference[band], 0.0f);
    EXPECT_FLOAT_EQ(out.band_agc_reference[band], out.band_energy[band]);
}

TEST(FeatureExtractorTest, AgcReferenceSnapsExactlyToRisingEnergy) {
    constexpr float kToneHz = 1000.0f;
    auto silence = fixtures::silence(audio_config::kFftWindowSize);
    auto tone = fixtures::sine_wave(kToneHz, kSampleRate, audio_config::kFftWindowSize);
    std::size_t band = band_for_frequency(kToneHz, kSampleRate);

    FeatureExtractor extractor(kSampleRate);
    AudioFeatures out;
    for (int i = 0; i < 200; ++i) {
        extractor.process(silence.data(), false, out);
    }

    float prev_energy = out.band_energy[band];
    for (int i = 0; i < 20; ++i) {
        extractor.process(tone.data(), false, out);
        if (out.band_energy[band] > prev_energy) {
            EXPECT_FLOAT_EQ(out.band_agc_reference[band], out.band_energy[band]);
        }
        prev_energy = out.band_energy[band];
    }
}

TEST(FeatureExtractorTest, AgcReferenceDecaysSlowerThanItRises) {
    constexpr float kToneHz = 1000.0f;
    auto tone = fixtures::sine_wave(kToneHz, kSampleRate, audio_config::kFftWindowSize);
    auto silence = fixtures::silence(audio_config::kFftWindowSize);
    std::size_t band = band_for_frequency(kToneHz, kSampleRate);

    FeatureExtractor extractor(kSampleRate);
    AudioFeatures out;

    for (int i = 0; i < 200; ++i) {
        extractor.process(tone.data(), false, out);
    }
    float peak_reference = out.band_agc_reference[band];
    ASSERT_GT(peak_reference, 0.0f);

    for (int i = 0; i < 200; ++i) {
        extractor.process(silence.data(), false, out);
    }

    EXPECT_LT(out.band_energy[band], peak_reference * 0.1f);
    EXPECT_GT(out.band_agc_reference[band], peak_reference * 0.8f);
}

TEST(FeatureExtractorTest, AgcReferenceNeverBelowFloorEvenOnSilence) {
    auto silence = fixtures::silence(audio_config::kFftWindowSize);
    FeatureExtractor extractor(kSampleRate);
    AudioFeatures out;

    for (int i = 0; i < 500; ++i) {
        extractor.process(silence.data(), false, out);
    }

    for (std::size_t b = 0; b < audio_config::kTotalBandSlots; ++b) {
        EXPECT_GE(out.band_agc_reference[b], audio_config::kAgcMinReference);
    }
}

TEST(FeatureExtractorTest, OnsetThresholdEnforcesAbsoluteFloorAfterQuietWarmup) {
    constexpr float kToneHz = 1000.0f;
    auto silence = fixtures::silence(audio_config::kFftWindowSize);
    auto spike = fixtures::sine_wave(kToneHz, kSampleRate, audio_config::kFftWindowSize, 0.0007f);
    std::size_t band = band_for_frequency(kToneHz, kSampleRate);

    FeatureExtractor extractor(kSampleRate);
    AudioFeatures out;
    for (std::size_t i = 0; i < audio_config::kOnsetHistoryLength + 10; ++i) {
        extractor.process(silence.data(), false, out);
    }

    extractor.process(spike.data(), false, out);

    EXPECT_FALSE(out.band_onset[band]);
}
