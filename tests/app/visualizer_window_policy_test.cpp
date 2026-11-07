#include "app/visualizer_window_policy.h"

#include <gtest/gtest.h>

TEST(VisualizerWindowPolicyTest, WindowedVisualizerDragsFromAnywhereInItsClientArea) {
    EXPECT_EQ(visualizer_window_policy::hit_region(false), VisualizerHitRegion::Caption);
}

TEST(VisualizerWindowPolicyTest, FullscreenVisualizerCannotBeDragged) {
    EXPECT_EQ(visualizer_window_policy::hit_region(true), VisualizerHitRegion::Client);
}

TEST(VisualizerWindowPolicyTest, WindowedVisualizerRequestsRoundedCorners) {
    EXPECT_EQ(visualizer_window_policy::corner_style(false), VisualizerCornerStyle::Rounded);
}

TEST(VisualizerWindowPolicyTest, FullscreenVisualizerIsSquare) {
    EXPECT_EQ(visualizer_window_policy::corner_style(true), VisualizerCornerStyle::Square);
}
