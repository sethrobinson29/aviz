#include "app/visualizer_window_policy.h"

#include <gtest/gtest.h>

TEST(VisualizerWindowPolicyTest, WindowedVisualizerDragsFromAnywhereOutsideTheOverlayButton) {
    EXPECT_EQ(visualizer_window_policy::hit_region(false, false), VisualizerHitRegion::Caption);
}

TEST(VisualizerWindowPolicyTest, OverlayButtonReceivesClicksInsteadOfStartingADrag) {
    EXPECT_EQ(visualizer_window_policy::hit_region(false, true), VisualizerHitRegion::Client);
}

TEST(VisualizerWindowPolicyTest, FullscreenVisualizerCannotBeDragged) {
    EXPECT_EQ(visualizer_window_policy::hit_region(true, false), VisualizerHitRegion::Client);
    EXPECT_EQ(visualizer_window_policy::hit_region(true, true), VisualizerHitRegion::Client);
}

TEST(VisualizerWindowPolicyTest, WindowedVisualizerRequestsRoundedCorners) {
    EXPECT_EQ(visualizer_window_policy::corner_style(false), VisualizerCornerStyle::Rounded);
}

TEST(VisualizerWindowPolicyTest, FullscreenVisualizerIsSquare) {
    EXPECT_EQ(visualizer_window_policy::corner_style(true), VisualizerCornerStyle::Square);
}
