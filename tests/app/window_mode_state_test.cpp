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

TEST(WindowModeStateTest, ExitingFullscreenWhileDetachedRestoresThePreFullscreenRectInsteadOfDocking) {
    WindowModeState state;
    state.set_docked(false);
    state.set_fullscreen(true);

    WindowLayoutAction action = state.set_fullscreen(false);

    EXPECT_TRUE(action.restore_windowed_rect);
    EXPECT_FALSE(action.reposition_docked);
}

TEST(WindowModeStateTest, ExitingFullscreenWhileDockedSnapsToDockedLayoutWithoutRestoringTheOldRect) {
    WindowModeState state;
    state.set_fullscreen(true);

    WindowLayoutAction action = state.set_fullscreen(false);

    EXPECT_TRUE(action.reposition_docked);
    EXPECT_FALSE(action.restore_windowed_rect);
}

TEST(WindowModeStateTest, OnlyExitingFullscreenEverRequestsARestore) {
    WindowModeState state;
    EXPECT_FALSE(state.set_docked(false).restore_windowed_rect);
    EXPECT_FALSE(state.set_fullscreen(true).restore_windowed_rect);
    EXPECT_FALSE(state.set_docked(true).restore_windowed_rect);
    EXPECT_FALSE(state.set_docked(false).restore_windowed_rect);
}

TEST(WindowModeStateTest, FullscreenChangesNeverChangeTheDockedMode) {
    WindowModeState docked;
    docked.set_fullscreen(true);
    EXPECT_EQ(docked.mode(), WindowMode::Docked);
    docked.set_fullscreen(false);
    EXPECT_EQ(docked.mode(), WindowMode::Docked);

    WindowModeState detached;
    detached.set_docked(false);
    detached.set_fullscreen(true);
    EXPECT_EQ(detached.mode(), WindowMode::Detached);
    detached.set_fullscreen(false);
    EXPECT_EQ(detached.mode(), WindowMode::Detached);
}
