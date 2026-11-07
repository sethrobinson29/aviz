#include "app/window_layout_config.h"

#include <limits>

#include <gtest/gtest.h>

TEST(ComputeDockedLayoutTest, VisualizerAndPanelAreAdjacentWithNoGap) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{0, 0, 1920, 1080}, 1.0f);

    EXPECT_EQ(layout.panel.x, layout.visualizer.x + layout.visualizer.width);
    EXPECT_EQ(layout.panel.y, layout.visualizer.y);
    EXPECT_EQ(layout.panel.height, layout.visualizer.height);
}

TEST(ComputeDockedLayoutTest, UsesTheConfiguredFixedDimensions) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{0, 0, 1920, 1080}, 1.0f);

    EXPECT_EQ(layout.visualizer.width, window_layout::kVisualizerWidth);
    EXPECT_EQ(layout.visualizer.height, window_layout::kVisualizerHeight);
    EXPECT_EQ(layout.panel.width, window_layout::kPanelWidthDip);
}

TEST(ComputeDockedLayoutTest, CentersTheCombinedBlockOnTheMonitor) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{0, 0, 1920, 1080}, 1.0f);

    const int combined_width = window_layout::kVisualizerWidth + window_layout::kPanelWidthDip;
    EXPECT_EQ(layout.visualizer.x, (1920 - combined_width) / 2);
    EXPECT_EQ(layout.visualizer.y, (1080 - window_layout::kVisualizerHeight) / 2);
}

TEST(ComputeDockedLayoutTest, ClampsToZeroWhenMonitorIsSmallerThanTheCombinedLayout) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{0, 0, 200, 200}, 1.0f);

    EXPECT_EQ(layout.visualizer.x, 0);
    EXPECT_EQ(layout.visualizer.y, 0);
    EXPECT_EQ(layout.panel.x, layout.visualizer.width);
}

TEST(ComputeDockedLayoutTest, PlacesTheBlockOnASecondaryMonitorRelativeToThatMonitorsOrigin) {
    DockedLayout primary = window_layout::compute_docked_layout(WindowRect{0, 0, 1920, 1080}, 1.0f);
    DockedLayout secondary = window_layout::compute_docked_layout(WindowRect{1920, 0, 1920, 1080}, 1.0f);

    EXPECT_EQ(secondary.visualizer.x, primary.visualizer.x + 1920);
    EXPECT_EQ(secondary.panel.x, primary.panel.x + 1920);
    EXPECT_EQ(secondary.visualizer.y, primary.visualizer.y);
}

TEST(ComputeDockedLayoutTest, HandlesMonitorsLeftOfAndAbovePrimaryWithNegativeOrigins) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{-1920, -200, 1920, 1080}, 1.0f);

    const int combined_width = window_layout::kVisualizerWidth + window_layout::kPanelWidthDip;
    EXPECT_EQ(layout.visualizer.x, -1920 + (1920 - combined_width) / 2);
    EXPECT_EQ(layout.visualizer.y, -200 + (1080 - window_layout::kVisualizerHeight) / 2);
    EXPECT_EQ(layout.panel.x, layout.visualizer.x + window_layout::kVisualizerWidth);
}

TEST(ComputeDockedLayoutTest, ClampsToTheMonitorOriginNotScreenZeroWhenTheMonitorIsTooSmall) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{2560, 300, 200, 200}, 1.0f);

    EXPECT_EQ(layout.visualizer.x, 2560);
    EXPECT_EQ(layout.visualizer.y, 300);
}

TEST(ComputeDockedLayoutTest, PanelWidthScalesWithThePanelMonitorsDpi) {
    EXPECT_EQ(window_layout::compute_docked_layout(WindowRect{0, 0, 3840, 2160}, 1.25f).panel.width, 525);
    EXPECT_EQ(window_layout::compute_docked_layout(WindowRect{0, 0, 3840, 2160}, 1.5f).panel.width, 630);
    EXPECT_EQ(window_layout::compute_docked_layout(WindowRect{0, 0, 3840, 2160}, 2.0f).panel.width, 840);
}

TEST(ComputeDockedLayoutTest, VisualizerKeepsItsFixedPhysicalSizeAtAnyDpi) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{0, 0, 3840, 2160}, 1.5f);

    EXPECT_EQ(layout.visualizer.width, window_layout::kVisualizerWidth);
    EXPECT_EQ(layout.visualizer.height, window_layout::kVisualizerHeight);
    EXPECT_EQ(layout.panel.height, window_layout::kVisualizerHeight);
}

TEST(ComputeDockedLayoutTest, CentersTheScaledBlockAndKeepsItAdjacent) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{0, 0, 2560, 1440}, 1.5f);

    EXPECT_EQ(layout.visualizer.x, (2560 - (window_layout::kVisualizerWidth + 630)) / 2);
    EXPECT_EQ(layout.panel.x, layout.visualizer.x + layout.visualizer.width);
}

TEST(ComputeDockedLayoutTest, InvalidScaleFallsBackToOne) {
    EXPECT_EQ(window_layout::panel_width_for_scale(0.0f), window_layout::kPanelWidthDip);
    EXPECT_EQ(window_layout::panel_width_for_scale(-1.5f), window_layout::kPanelWidthDip);
    EXPECT_EQ(window_layout::panel_width_for_scale(std::numeric_limits<float>::quiet_NaN()),
              window_layout::kPanelWidthDip);
    EXPECT_EQ(window_layout::panel_width_for_scale(std::numeric_limits<float>::infinity()),
              window_layout::kPanelWidthDip);
}

TEST(ComputeDockedLayoutTest, PanelAtEveryTestedScaleIsAtLeastTheMinimumCssWidth) {
    for (float scale : {1.0f, 1.25f, 1.5f, 1.75f, 2.0f}) {
        EXPECT_GE(static_cast<float>(window_layout::panel_width_for_scale(scale)) / scale, 370.0f) << scale;
    }
}
