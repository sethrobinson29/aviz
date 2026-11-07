#include "app/visualizer_overlay.h"

#include <cmath>

#include <gtest/gtest.h>

namespace {

const VisualizerChromeTheme& theme() { return visualizer_chrome_theme::kDarkY2k; }

OverlayPointer pointer_at(float x, float y, bool pressed = false, bool released = false) {
    return OverlayPointer{true, x, y, pressed, released};
}

const OverlayRect kButton{100.0f, 50.0f, 26.0f, 26.0f};

}

TEST(VisualizerOverlayLayoutTest, WindowedShowsTheFullscreenToggleInTheTopRightCorner) {
    const OverlayLayout layout = visualizer_overlay::layout(theme(), false, 1280.0f, 720.0f, 1.0f, 90.0f, 12.0f);
    EXPECT_EQ(layout.button, OverlayButton::EnterFullscreen);
    EXPECT_FLOAT_EQ(layout.rect.width, 26.0f);
    EXPECT_FLOAT_EQ(layout.rect.height, 26.0f);
    EXPECT_FLOAT_EQ(layout.rect.x, 1280.0f - 16.0f - 26.0f);
    EXPECT_FLOAT_EQ(layout.rect.y, 16.0f);
}

TEST(VisualizerOverlayLayoutTest, ToggleIconIsCenteredInsideTheButton) {
    const OverlayLayout layout = visualizer_overlay::layout(theme(), false, 1280.0f, 720.0f, 1.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(layout.icon_in_button.width, 13.0f);
    EXPECT_FLOAT_EQ(layout.icon_in_button.x, 7.0f);
    EXPECT_FLOAT_EQ(layout.icon_in_button.y, 7.0f);
}

TEST(VisualizerOverlayLayoutTest, FullscreenShowsTheExitPillInTheBottomRightCorner) {
    const OverlayLayout layout = visualizer_overlay::layout(theme(), true, 1920.0f, 1080.0f, 1.0f, 90.0f, 12.0f);
    EXPECT_EQ(layout.button, OverlayButton::ExitFullscreen);
    EXPECT_FLOAT_EQ(layout.rect.height, 10.0f + 14.0f + 10.0f);
    EXPECT_FLOAT_EQ(layout.rect.width, 14.0f + 14.0f + 8.0f + 90.0f + 18.0f);
    EXPECT_FLOAT_EQ(layout.rect.x + layout.rect.width, 1920.0f - 28.0f);
    EXPECT_FLOAT_EQ(layout.rect.y + layout.rect.height, 1080.0f - 28.0f);
}

TEST(VisualizerOverlayLayoutTest, PillLabelSitsAfterTheIconAndGapAndIsVerticallyCentered) {
    const OverlayLayout layout = visualizer_overlay::layout(theme(), true, 1920.0f, 1080.0f, 1.0f, 90.0f, 12.0f);
    EXPECT_FLOAT_EQ(layout.label_x, layout.rect.x + 14.0f + 14.0f + 8.0f);
    EXPECT_FLOAT_EQ(layout.label_y, layout.rect.y + 11.0f);
    EXPECT_FLOAT_EQ(layout.icon_in_button.x, 14.0f);
    EXPECT_FLOAT_EQ(layout.icon_in_button.y, 10.0f);
}

TEST(VisualizerOverlayLayoutTest, TallerLabelThanIconGrowsThePill) {
    const OverlayLayout layout = visualizer_overlay::layout(theme(), true, 1920.0f, 1080.0f, 1.0f, 90.0f, 20.0f);
    EXPECT_FLOAT_EQ(layout.rect.height, 10.0f + 20.0f + 10.0f);
}

TEST(VisualizerOverlayLayoutTest, SizesAndMarginsScaleWithDpi) {
    const OverlayLayout toggle = visualizer_overlay::layout(theme(), false, 1920.0f, 1080.0f, 1.5f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(toggle.rect.width, 39.0f);
    EXPECT_FLOAT_EQ(toggle.rect.y, 24.0f);
    EXPECT_FLOAT_EQ(toggle.rect.x, 1920.0f - 24.0f - 39.0f);

    const OverlayLayout pill = visualizer_overlay::layout(theme(), true, 1920.0f, 1080.0f, 1.5f, 135.0f, 18.0f);
    EXPECT_FLOAT_EQ(pill.rect.height, 15.0f + 21.0f + 15.0f);
    EXPECT_FLOAT_EQ(pill.rect.y + pill.rect.height, 1080.0f - 42.0f);
}

TEST(VisualizerOverlayLayoutTest, ButtonEdgesLandOnWholePixelsAtFractionalScale) {
    const OverlayLayout pill = visualizer_overlay::layout(theme(), true, 1920.0f, 1080.0f, 1.25f, 101.3f, 15.0f);
    EXPECT_FLOAT_EQ(pill.rect.width, std::round(pill.rect.width));
    EXPECT_FLOAT_EQ(pill.rect.height, std::round(pill.rect.height));
    EXPECT_FLOAT_EQ(pill.rect.x, std::round(pill.rect.x));
    EXPECT_FLOAT_EQ(pill.rect.y, std::round(pill.rect.y));
}

TEST(VisualizerOverlayLayoutTest, NonPositiveScaleFallsBackToOne) {
    const OverlayLayout zero = visualizer_overlay::layout(theme(), false, 1280.0f, 720.0f, 0.0f, 0.0f, 0.0f);
    const OverlayLayout negative = visualizer_overlay::layout(theme(), false, 1280.0f, 720.0f, -2.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(zero.rect.width, 26.0f);
    EXPECT_FLOAT_EQ(negative.rect.width, 26.0f);
}

TEST(VisualizerOverlayRectTest, ContainsIsHalfOpen) {
    EXPECT_TRUE(kButton.contains(100.0f, 50.0f));
    EXPECT_TRUE(kButton.contains(125.9f, 75.9f));
    EXPECT_FALSE(kButton.contains(126.0f, 60.0f));
    EXPECT_FALSE(kButton.contains(110.0f, 76.0f));
    EXPECT_FALSE(kButton.contains(99.9f, 60.0f));
}

TEST(VisualizerOverlayStepTest, PressAndReleaseInsideActivatesOnRelease) {
    OverlayStep pressed = visualizer_overlay::step(OverlayButtonState{}, kButton, pointer_at(110.0f, 60.0f, true));
    EXPECT_FALSE(pressed.activated);
    EXPECT_TRUE(pressed.state.armed);
    OverlayStep released = visualizer_overlay::step(pressed.state, kButton, pointer_at(112.0f, 62.0f, false, true));
    EXPECT_TRUE(released.activated);
    EXPECT_FALSE(released.state.armed);
}

TEST(VisualizerOverlayStepTest, PressInsideThenReleaseOutsideDoesNotActivate) {
    OverlayStep pressed = visualizer_overlay::step(OverlayButtonState{}, kButton, pointer_at(110.0f, 60.0f, true));
    OverlayStep released = visualizer_overlay::step(pressed.state, kButton, pointer_at(300.0f, 300.0f, false, true));
    EXPECT_FALSE(released.activated);
    EXPECT_FALSE(released.state.armed);
}

TEST(VisualizerOverlayStepTest, PressOutsideThenReleaseInsideDoesNotActivate) {
    OverlayStep pressed = visualizer_overlay::step(OverlayButtonState{}, kButton, pointer_at(10.0f, 10.0f, true));
    EXPECT_FALSE(pressed.state.armed);
    OverlayStep released = visualizer_overlay::step(pressed.state, kButton, pointer_at(110.0f, 60.0f, false, true));
    EXPECT_FALSE(released.activated);
}

TEST(VisualizerOverlayStepTest, ReleaseWithoutAnyPressDoesNotActivate) {
    OverlayStep released =
        visualizer_overlay::step(OverlayButtonState{}, kButton, pointer_at(110.0f, 60.0f, false, true));
    EXPECT_FALSE(released.activated);
}

TEST(VisualizerOverlayStepTest, PressAndReleaseInTheSameFrameInsideActivates) {
    OverlayStep click = visualizer_overlay::step(OverlayButtonState{}, kButton, pointer_at(110.0f, 60.0f, true, true));
    EXPECT_TRUE(click.activated);
}

TEST(VisualizerOverlayStepTest, HoverTracksThePointer) {
    OverlayStep inside = visualizer_overlay::step(OverlayButtonState{}, kButton, pointer_at(110.0f, 60.0f));
    EXPECT_TRUE(inside.state.hovered);
    OverlayStep outside = visualizer_overlay::step(inside.state, kButton, pointer_at(10.0f, 60.0f));
    EXPECT_FALSE(outside.state.hovered);
}

TEST(VisualizerOverlayStepTest, LeavingTheClientAreaClearsHoverAndCancelsAPress) {
    OverlayStep pressed = visualizer_overlay::step(OverlayButtonState{}, kButton, pointer_at(110.0f, 60.0f, true));
    OverlayStep left =
        visualizer_overlay::step(pressed.state, kButton, OverlayPointer{false, 110.0f, 60.0f, false, false});
    EXPECT_FALSE(left.state.hovered);
    EXPECT_FALSE(left.state.armed);
    OverlayStep back = visualizer_overlay::step(left.state, kButton, pointer_at(110.0f, 60.0f, false, true));
    EXPECT_FALSE(back.activated);
}

TEST(VisualizerOverlayStepTest, ReleaseWhileOffScreenNeverActivates) {
    OverlayStep pressed = visualizer_overlay::step(OverlayButtonState{}, kButton, pointer_at(110.0f, 60.0f, true));
    OverlayStep released =
        visualizer_overlay::step(pressed.state, kButton, OverlayPointer{false, 110.0f, 60.0f, false, true});
    EXPECT_FALSE(released.activated);
}
