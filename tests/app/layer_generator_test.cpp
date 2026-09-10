#include "app/layer_generator.h"

#include <array>
#include <random>
#include <set>

#include <gtest/gtest.h>

#include "audio/audio_config.h"

namespace {

TEST(LayerGeneratorTest, CountAboveBandCountCoversAllBandsVisualsAndReactivitySources) {
    std::mt19937 rng(12345);
    std::vector<GeneratedLayerSpec> specs = generate_layer_specs(24, rng);

    ASSERT_EQ(specs.size(), 24u);

    std::set<std::size_t> bands_seen;
    std::set<VisualType> visuals_seen;
    bool global_beat_on_global_layer = false;
    bool band_onset_seen = false;
    bool band_amplitude_seen = false;

    for (const GeneratedLayerSpec& spec : specs) {
        visuals_seen.insert(spec.visual_type);
        if (spec.freq_band.has_value()) {
            bands_seen.insert(*spec.freq_band);
            EXPECT_NE(spec.reactivity_source, ReactivitySource::GlobalBeat)
                << "band-scoped layer must not use GlobalBeat";
            if (spec.reactivity_source == ReactivitySource::BandOnset) {
                band_onset_seen = true;
            } else if (spec.reactivity_source == ReactivitySource::BandAmplitude) {
                band_amplitude_seen = true;
            }
        } else {
            EXPECT_EQ(spec.reactivity_source, ReactivitySource::GlobalBeat)
                << "Global-scoped layer must use GlobalBeat";
            global_beat_on_global_layer = true;
        }
    }

    EXPECT_EQ(bands_seen.size(), audio_config::kNumBands) << "every real band must be represented";
    EXPECT_TRUE(visuals_seen.count(VisualType::Waveform));
    EXPECT_TRUE(visuals_seen.count(VisualType::Spectrum));
    EXPECT_TRUE(visuals_seen.count(VisualType::Radial));
    EXPECT_TRUE(global_beat_on_global_layer);
    EXPECT_TRUE(band_onset_seen);
    EXPECT_TRUE(band_amplitude_seen);
}

TEST(LayerGeneratorTest, CountAtOrBelowBandCountSkipsBandCoverageGuarantee) {
    std::mt19937 rng(6789);
    std::vector<GeneratedLayerSpec> specs = generate_layer_specs(audio_config::kNumBands, rng);

    ASSERT_EQ(specs.size(), audio_config::kNumBands);
    for (const GeneratedLayerSpec& spec : specs) {
        if (spec.freq_band.has_value()) {
            EXPECT_LT(*spec.freq_band, audio_config::kNumBands);
            EXPECT_NE(spec.reactivity_source, ReactivitySource::GlobalBeat);
        } else {
            EXPECT_EQ(spec.reactivity_source, ReactivitySource::GlobalBeat);
        }
    }
}

TEST(LayerGeneratorTest, ZeroCountReturnsEmpty) {
    std::mt19937 rng(1);
    EXPECT_TRUE(generate_layer_specs(0, rng).empty());
}

}
