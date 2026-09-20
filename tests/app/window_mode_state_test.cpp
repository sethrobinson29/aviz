#include "app/window_mode_state.h"

#include <gtest/gtest.h>

TEST(WindowModeStateTest, StartsDockedAndNotFullscreen) {
    WindowModeState state;
    EXPECT_EQ(state.mode(), WindowMode::Docked);
    EXPECT_FALSE(state.is_fullscreen());
}

TEST(WindowModeStateTest, SetDockedTrueWhenAlreadyDockedIsANoOp) {
    WindowModeState state;
    WindowLayoutAction action = state.set_docked(true);

    EXPECT_EQ(state.mode(), WindowMode::Docked);
    EXPECT_FALSE(action.reposition_docked);
    EXPECT_FALSE(action.show_panel);
    EXPECT_FALSE(action.hide_panel);
}

TEST(WindowModeStateTest, DetachingRequestsNoRepositioning) {
    WindowModeState state;
    WindowLayoutAction action = state.set_docked(false);

    EXPECT_EQ(state.mode(), WindowMode::Detached);
    EXPECT_FALSE(action.reposition_docked);
}

TEST(WindowModeStateTest, RedockingRequestsReposition) {
    WindowModeState state;
    state.set_docked(false);
    WindowLayoutAction action = state.set_docked(true);

    EXPECT_EQ(state.mode(), WindowMode::Docked);
    EXPECT_TRUE(action.reposition_docked);
}

TEST(WindowModeStateTest, SetDetachedWhenAlreadyDetachedIsANoOp) {
    WindowModeState state;
    state.set_docked(false);
    WindowLayoutAction action = state.set_docked(false);

    EXPECT_FALSE(action.reposition_docked);
}

TEST(WindowModeStateTest, EnteringFullscreenWhileDockedHidesThePanel) {
    WindowModeState state;
    WindowLayoutAction action = state.set_fullscreen(true);

    EXPECT_TRUE(state.is_fullscreen());
    EXPECT_TRUE(action.enter_fullscreen);
    EXPECT_TRUE(action.hide_panel);
    EXPECT_FALSE(action.show_panel);
}

TEST(WindowModeStateTest, ExitingFullscreenWhileDockedShowsPanelAndReposition) {
    WindowModeState state;
    state.set_fullscreen(true);
    WindowLayoutAction action = state.set_fullscreen(false);

    EXPECT_FALSE(state.is_fullscreen());
    EXPECT_TRUE(action.exit_fullscreen);
    EXPECT_TRUE(action.show_panel);
    EXPECT_TRUE(action.reposition_docked);
}

TEST(WindowModeStateTest, EnteringFullscreenWhileDetachedLeavesThePanelVisible) {
    WindowModeState state;
    state.set_docked(false);
    WindowLayoutAction action = state.set_fullscreen(true);

    EXPECT_TRUE(action.enter_fullscreen);
    EXPECT_FALSE(action.hide_panel);
    EXPECT_FALSE(action.show_panel);
}

TEST(WindowModeStateTest, ExitingFullscreenWhileDetachedDoesNotReshowOrReposition) {
    WindowModeState state;
    state.set_docked(false);
    state.set_fullscreen(true);
    WindowLayoutAction action = state.set_fullscreen(false);

    EXPECT_TRUE(action.exit_fullscreen);
    EXPECT_FALSE(action.show_panel) << "panel was never hidden while detached, so there's nothing to re-show";
    EXPECT_FALSE(action.reposition_docked);
}

TEST(WindowModeStateTest, SetFullscreenToItsCurrentValueIsANoOp) {
    WindowModeState state;
    WindowLayoutAction action = state.set_fullscreen(false);

    EXPECT_FALSE(action.enter_fullscreen);
    EXPECT_FALSE(action.exit_fullscreen);
    EXPECT_FALSE(action.hide_panel);
    EXPECT_FALSE(action.show_panel);
}

TEST(WindowModeStateTest, TogglingDockWhileFullscreenDefersRepositioning) {
    WindowModeState state;
    state.set_fullscreen(true);
    WindowLayoutAction action = state.set_docked(false);
    WindowLayoutAction redock_action = state.set_docked(true);

    EXPECT_FALSE(action.reposition_docked);
    EXPECT_FALSE(redock_action.reposition_docked) << "still fullscreen -- reposition should wait for exit";
    EXPECT_EQ(state.mode(), WindowMode::Docked);
}

TEST(WindowModeStateTest, RepositionAppliesOnceFullscreenExitsAfterDockedWhileFullscreen) {
    WindowModeState state;
    state.set_fullscreen(true);
    state.set_docked(true);
    WindowLayoutAction exit_action = state.set_fullscreen(false);

    EXPECT_TRUE(exit_action.reposition_docked);
}

TEST(WindowModeStateTest, DetachingWhileFullscreenAndDockedShowsThePanelImmediately) {
    WindowModeState state;
    state.set_fullscreen(true);
    WindowLayoutAction action = state.set_docked(false);

    EXPECT_TRUE(action.show_panel) << "still fullscreen, but now detached -- panel isn't in the way anymore";
    EXPECT_FALSE(action.reposition_docked);
}

TEST(WindowModeStateTest, RedockingWhileFullscreenAndDetachedHidesThePanelImmediately) {
    WindowModeState state;
    state.set_docked(false);
    state.set_fullscreen(true);
    WindowLayoutAction action = state.set_docked(true);

    EXPECT_TRUE(action.hide_panel) << "still fullscreen, and now docked -- panel would be in the way";
    EXPECT_FALSE(action.reposition_docked) << "still fullscreen -- reposition should wait for exit";
}
