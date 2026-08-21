#include "compositor/compositor_math.h"

#include <gtest/gtest.h>

#include "raylib.h"
#include "rlgl.h"

TEST(ClampOpacityTest, ClampsIntoZeroOneRange) {
    EXPECT_FLOAT_EQ(compositor_math::clamp_opacity(-0.5f), 0.0f);
    EXPECT_FLOAT_EQ(compositor_math::clamp_opacity(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(compositor_math::clamp_opacity(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(compositor_math::clamp_opacity(1.0f), 1.0f);
    EXPECT_FLOAT_EQ(compositor_math::clamp_opacity(1.5f), 1.0f);
}

TEST(ToBlendFactorsTest, BuiltInModesMapToRaylibConstants) {
    EXPECT_EQ(compositor_math::to_blend_factors(LayerBlendMode::Normal).raylib_mode, BLEND_ALPHA);
    EXPECT_EQ(compositor_math::to_blend_factors(LayerBlendMode::Additive).raylib_mode, BLEND_ADDITIVE);
    EXPECT_EQ(compositor_math::to_blend_factors(LayerBlendMode::Multiply).raylib_mode,
              BLEND_MULTIPLIED);
}

TEST(ToBlendFactorsTest, ScreenModeIsCustomWithScreenFormulaFactors) {
    const auto factors = compositor_math::to_blend_factors(LayerBlendMode::Screen);
    EXPECT_EQ(factors.raylib_mode, BLEND_CUSTOM);
    // result = src*(1-dst) + dst*1 = src + dst - src*dst
    EXPECT_EQ(factors.src_factor, RL_ONE_MINUS_DST_COLOR);
    EXPECT_EQ(factors.dst_factor, RL_ONE);
    EXPECT_EQ(factors.equation, RL_FUNC_ADD);
}

TEST(MoveElementTest, NoOpOnEmptyVector) {
    std::vector<int> v;
    compositor_math::move_element(v, 0, 0);
    EXPECT_TRUE(v.empty());
}

TEST(MoveElementTest, NoOpWhenFromEqualsTo) {
    std::vector<int> v{1, 2, 3};
    compositor_math::move_element(v, 1, 1);
    EXPECT_EQ(v, (std::vector<int>{1, 2, 3}));
}

TEST(MoveElementTest, MovesForward) {
    std::vector<int> v{1, 2, 3, 4};
    compositor_math::move_element(v, 0, 2);
    EXPECT_EQ(v, (std::vector<int>{2, 3, 1, 4}));
}

TEST(MoveElementTest, MovesBackward) {
    std::vector<int> v{1, 2, 3, 4};
    compositor_math::move_element(v, 3, 1);
    EXPECT_EQ(v, (std::vector<int>{1, 4, 2, 3}));
}

TEST(MoveElementTest, ClampsOutOfRangeIndicesToLastElement) {
    std::vector<int> v{1, 2, 3};
    compositor_math::move_element(v, 99, 0);
    EXPECT_EQ(v, (std::vector<int>{3, 1, 2}));
}
