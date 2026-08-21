#include "compositor/layer.h"

#include <gtest/gtest.h>

#include "visuals/spectrum/spectrum_visual.h"
#include "visuals/waveform/waveform_visual.h"

TEST(MakeVisualTest, WaveformTypeConstructsWaveformVisual) {
    auto visual = make_visual(VisualType::Waveform);
    EXPECT_NE(dynamic_cast<WaveformVisual*>(visual.get()), nullptr);
    EXPECT_EQ(dynamic_cast<SpectrumVisual*>(visual.get()), nullptr);
}

TEST(MakeVisualTest, SpectrumTypeConstructsSpectrumVisual) {
    auto visual = make_visual(VisualType::Spectrum);
    EXPECT_NE(dynamic_cast<SpectrumVisual*>(visual.get()), nullptr);
    EXPECT_EQ(dynamic_cast<WaveformVisual*>(visual.get()), nullptr);
}
