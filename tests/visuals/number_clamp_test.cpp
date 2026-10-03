#include "visuals/number_clamp.h"

#include <limits>

#include <gtest/gtest.h>

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

}

TEST(ClampNumberToRangeIntTest, InRangeValueTruncatesTowardZero) {
    EXPECT_EQ(number_math::clamp_number_to_range<int>(150.9, 10, 600), 150);
    EXPECT_EQ(number_math::clamp_number_to_range<int>(-3.9, -10, 10), -3);
}

TEST(ClampNumberToRangeIntTest, BoundsThemselvesAreReturnedExactly) {
    EXPECT_EQ(number_math::clamp_number_to_range<int>(10.0, 10, 600), 10);
    EXPECT_EQ(number_math::clamp_number_to_range<int>(600.0, 10, 600), 600);
}

TEST(ClampNumberToRangeIntTest, OutOfRangeHighAndLowClampToBounds) {
    EXPECT_EQ(number_math::clamp_number_to_range<int>(601.0, 10, 600), 600);
    EXPECT_EQ(number_math::clamp_number_to_range<int>(9.99, 10, 600), 10);
    EXPECT_EQ(number_math::clamp_number_to_range<int>(1e30, 10, 600), 600);
    EXPECT_EQ(number_math::clamp_number_to_range<int>(-1e30, 10, 600), 10);
    EXPECT_EQ(number_math::clamp_number_to_range<int>(1e300, 10, 600), 600);
}

TEST(ClampNumberToRangeIntTest, NaNAndNegativeInfinityClampToMinPositiveInfinityToMax) {
    EXPECT_EQ(number_math::clamp_number_to_range<int>(kNaN, 10, 600), 10);
    EXPECT_EQ(number_math::clamp_number_to_range<int>(-kInfinity, 10, 600), 10);
    EXPECT_EQ(number_math::clamp_number_to_range<int>(kInfinity, 10, 600), 600);
}

TEST(ClampNumberToRangeFloatTest, InRangeValueNarrowsToNearestFloat) {
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_range<float>(0.25, 0.0f, 1.0f), 0.25f);
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_range<float>(2.5, 0.0f, 4.0f), 2.5f);
}

TEST(ClampNumberToRangeFloatTest, OutOfRangeHighAndLowClampToBounds) {
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_range<float>(4.01, 0.0f, 4.0f), 4.0f);
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_range<float>(-0.01, 0.0f, 4.0f), 0.0f);
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_range<float>(1e300, 0.0f, 4.0f), 4.0f);
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_range<float>(-1e300, 0.0f, 4.0f), 0.0f);
}

TEST(ClampNumberToRangeFloatTest, NaNAndNegativeInfinityClampToMinPositiveInfinityToMax) {
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_range<float>(kNaN, 0.0f, 4.0f), 0.0f);
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_range<float>(-kInfinity, 0.0f, 4.0f), 0.0f);
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_range<float>(kInfinity, 0.0f, 4.0f), 4.0f);
}

TEST(ClampNumberToRepresentableTest, DoubleBeyondFloatRangeBecomesLargestFiniteFloatNotInfinity) {
    EXPECT_EQ(number_math::clamp_number_to_representable<float>(1e300), std::numeric_limits<float>::max());
    EXPECT_EQ(number_math::clamp_number_to_representable<float>(-1e300), std::numeric_limits<float>::lowest());
    EXPECT_EQ(number_math::clamp_number_to_representable<float>(kInfinity), std::numeric_limits<float>::max());
}

TEST(ClampNumberToRepresentableTest, OrdinaryValuesPassThroughUnchanged) {
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_representable<float>(0.5), 0.5f);
    EXPECT_FLOAT_EQ(number_math::clamp_number_to_representable<float>(-1234.25), -1234.25f);
    EXPECT_EQ(number_math::clamp_number_to_representable<int>(42.0), 42);
}

TEST(ClampNumberToRepresentableTest, DoubleBeyondIntRangeClampsToIntLimits) {
    EXPECT_EQ(number_math::clamp_number_to_representable<int>(1e30), std::numeric_limits<int>::max());
    EXPECT_EQ(number_math::clamp_number_to_representable<int>(-1e30), std::numeric_limits<int>::lowest());
    EXPECT_EQ(number_math::clamp_number_to_representable<int>(kNaN), std::numeric_limits<int>::lowest());
}
