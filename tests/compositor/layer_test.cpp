#include "compositor/layer.h"

#include <gtest/gtest.h>

#include "visuals/radial/radial_visual.h"
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

TEST(MakeVisualTest, RadialTypeConstructsRadialVisual) {
    auto visual = make_visual(VisualType::Radial);
    EXPECT_NE(dynamic_cast<RadialVisual*>(visual.get()), nullptr);
    EXPECT_EQ(dynamic_cast<SpectrumVisual*>(visual.get()), nullptr);
}

TEST(VisualCloneTest, WaveformCloneIsIndependentInstanceOfSameType) {
    auto original = make_visual(VisualType::Waveform);
    auto clone = original->clone();

    EXPECT_NE(clone.get(), original.get());
    EXPECT_NE(dynamic_cast<WaveformVisual*>(clone.get()), nullptr);
}

TEST(VisualCloneTest, WaveformClonePreservesParams) {
    WaveformVisual original(WaveformParams{BLUE, 5.0f});
    auto clone = original.clone();

    auto* waveform_clone = dynamic_cast<WaveformVisual*>(clone.get());
    ASSERT_NE(waveform_clone, nullptr);
    const WaveformParams& params = waveform_clone->params();
    EXPECT_EQ(params.line_color.r, BLUE.r);
    EXPECT_EQ(params.line_color.g, BLUE.g);
    EXPECT_EQ(params.line_color.b, BLUE.b);
    EXPECT_FLOAT_EQ(params.line_thickness, 5.0f);
}

TEST(VisualCloneTest, SpectrumCloneIsIndependentInstanceOfSameType) {
    auto original = make_visual(VisualType::Spectrum);
    auto clone = original->clone();

    EXPECT_NE(clone.get(), original.get());
    EXPECT_NE(dynamic_cast<SpectrumVisual*>(clone.get()), nullptr);
}

TEST(VisualCloneTest, SpectrumClonePreservesParams) {
    SpectrumVisual original(SpectrumParams{RED, 0.5f, 0.9f});
    auto clone = original.clone();

    auto* spectrum_clone = dynamic_cast<SpectrumVisual*>(clone.get());
    ASSERT_NE(spectrum_clone, nullptr);
    const SpectrumParams& params = spectrum_clone->params();
    EXPECT_EQ(params.bar_color.r, RED.r);
    EXPECT_EQ(params.bar_color.g, RED.g);
    EXPECT_EQ(params.bar_color.b, RED.b);
    EXPECT_FLOAT_EQ(params.gap_ratio, 0.5f);
    EXPECT_FLOAT_EQ(params.max_height_fraction, 0.9f);
}

TEST(VisualCloneTest, RadialCloneIsIndependentInstanceOfSameType) {
    auto original = make_visual(VisualType::Radial);
    auto clone = original->clone();

    EXPECT_NE(clone.get(), original.get());
    EXPECT_NE(dynamic_cast<RadialVisual*>(clone.get()), nullptr);
}

TEST(VisualCloneTest, RadialClonePreservesParams) {
    RadialVisual original(RadialParams{GREEN, 4.0f, 0.2f, 0.8f});
    auto clone = original.clone();

    auto* radial_clone = dynamic_cast<RadialVisual*>(clone.get());
    ASSERT_NE(radial_clone, nullptr);
    const RadialParams& params = radial_clone->params();
    EXPECT_EQ(params.bar_color.r, GREEN.r);
    EXPECT_EQ(params.bar_color.g, GREEN.g);
    EXPECT_EQ(params.bar_color.b, GREEN.b);
    EXPECT_FLOAT_EQ(params.thickness, 4.0f);
    EXPECT_FLOAT_EQ(params.inner_radius_fraction, 0.2f);
    EXPECT_FLOAT_EQ(params.max_radius_fraction, 0.8f);
}
