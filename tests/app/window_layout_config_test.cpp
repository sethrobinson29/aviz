#include "app/window_layout_config.h"

#include <gtest/gtest.h>

TEST(ComputeDockedLayoutTest, VisualizerAndPanelAreAdjacentWithNoGap) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{0, 0, 1920, 1080});

    EXPECT_EQ(layout.panel.x, layout.visualizer.x + layout.visualizer.width);
    EXPECT_EQ(layout.panel.y, layout.visualizer.y);
    EXPECT_EQ(layout.panel.height, layout.visualizer.height);
}

TEST(ComputeDockedLayoutTest, UsesTheConfiguredFixedDimensions) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{0, 0, 1920, 1080});

    EXPECT_EQ(layout.visualizer.width, window_layout::kVisualizerWidth);
    EXPECT_EQ(layout.visualizer.height, window_layout::kVisualizerHeight);
    EXPECT_EQ(layout.panel.width, window_layout::kPanelWidth);
}

TEST(ComputeDockedLayoutTest, CentersTheCombinedBlockOnTheMonitor) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{0, 0, 1920, 1080});

    const int combined_width = window_layout::kVisualizerWidth + window_layout::kPanelWidth;
    EXPECT_EQ(layout.visualizer.x, (1920 - combined_width) / 2);
    EXPECT_EQ(layout.visualizer.y, (1080 - window_layout::kVisualizerHeight) / 2);
}

TEST(ComputeDockedLayoutTest, ClampsToZeroWhenMonitorIsSmallerThanTheCombinedLayout) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{0, 0, 200, 200});

    EXPECT_EQ(layout.visualizer.x, 0);
    EXPECT_EQ(layout.visualizer.y, 0);
    EXPECT_EQ(layout.panel.x, layout.visualizer.width);
}

TEST(ComputeDockedLayoutTest, PlacesTheBlockOnASecondaryMonitorRelativeToThatMonitorsOrigin) {
    DockedLayout primary = window_layout::compute_docked_layout(WindowRect{0, 0, 1920, 1080});
    DockedLayout secondary = window_layout::compute_docked_layout(WindowRect{1920, 0, 1920, 1080});

    EXPECT_EQ(secondary.visualizer.x, primary.visualizer.x + 1920);
    EXPECT_EQ(secondary.panel.x, primary.panel.x + 1920);
    EXPECT_EQ(secondary.visualizer.y, primary.visualizer.y);
}

TEST(ComputeDockedLayoutTest, HandlesMonitorsLeftOfAndAbovePrimaryWithNegativeOrigins) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{-1920, -200, 1920, 1080});

    const int combined_width = window_layout::kVisualizerWidth + window_layout::kPanelWidth;
    EXPECT_EQ(layout.visualizer.x, -1920 + (1920 - combined_width) / 2);
    EXPECT_EQ(layout.visualizer.y, -200 + (1080 - window_layout::kVisualizerHeight) / 2);
    EXPECT_EQ(layout.panel.x, layout.visualizer.x + window_layout::kVisualizerWidth);
}

TEST(ComputeDockedLayoutTest, ClampsToTheMonitorOriginNotScreenZeroWhenTheMonitorIsTooSmall) {
    DockedLayout layout = window_layout::compute_docked_layout(WindowRect{2560, 300, 200, 200});

    EXPECT_EQ(layout.visualizer.x, 2560);
    EXPECT_EQ(layout.visualizer.y, 300);
}
