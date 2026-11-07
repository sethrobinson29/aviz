#include "app/chrome_raster.h"

#include <gtest/gtest.h>

namespace {

constexpr ChromeColor kClear{0, 0, 0, 0};
constexpr ChromeColor kFill{40, 50, 60, 255};
constexpr ChromeColor kFillBottom{10, 20, 30, 255};
constexpr ChromeColor kBorder{200, 100, 50, 255};
constexpr ChromeColor kWhite{255, 255, 255, 255};

ChromeButtonSpec plain_button(int width, int height, float radius, float border) {
    return ChromeButtonSpec{width, height, radius, border, 0.0f, kFill, kFill, kBorder, kClear,
                            ChromeIcon{{}, ChromePoint{0.0f, 0.0f}, 0.0f, 0.0f, kClear}};
}

bool same(ChromeColor a, ChromeColor b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

}

TEST(ChromeRasterDistanceTest, RoundedRectDistanceIsNegativeInsideZeroOnEdgePositiveOutside) {
    EXPECT_LT(chrome_raster::rounded_rect_distance({10.0f, 10.0f}, 20.0f, 20.0f, 4.0f), 0.0f);
    EXPECT_NEAR(chrome_raster::rounded_rect_distance({10.0f, 0.0f}, 20.0f, 20.0f, 4.0f), 0.0f, 1e-5f);
    EXPECT_GT(chrome_raster::rounded_rect_distance({10.0f, -3.0f}, 20.0f, 20.0f, 4.0f), 2.9f);
}

TEST(ChromeRasterDistanceTest, RoundedCornerIsCutAwayFromTheSquareCorner) {
    EXPECT_GT(chrome_raster::rounded_rect_distance({0.5f, 0.5f}, 20.0f, 20.0f, 6.0f), 0.0f);
    EXPECT_LT(chrome_raster::rounded_rect_distance({0.5f, 0.5f}, 20.0f, 20.0f, 0.0f), 0.0f);
}

TEST(ChromeRasterDistanceTest, RadiusLargerThanHalfTheShortSideClampsToACapsule) {
    const float clamped = chrome_raster::rounded_rect_distance({0.0f, 10.0f}, 60.0f, 20.0f, 500.0f);
    const float capsule = chrome_raster::rounded_rect_distance({0.0f, 10.0f}, 60.0f, 20.0f, 10.0f);
    EXPECT_FLOAT_EQ(clamped, capsule);
}

TEST(ChromeRasterDistanceTest, SegmentDistanceMeasuresToTheClosestPointIncludingEnds) {
    const ChromeSegment segment{{0.0f, 0.0f}, {10.0f, 0.0f}};
    EXPECT_FLOAT_EQ(chrome_raster::segment_distance({5.0f, 3.0f}, segment), 3.0f);
    EXPECT_FLOAT_EQ(chrome_raster::segment_distance({13.0f, 4.0f}, segment), 5.0f);
    EXPECT_FLOAT_EQ(chrome_raster::segment_distance({2.0f, 2.0f}, ChromeSegment{{2.0f, 0.0f}, {2.0f, 0.0f}}), 2.0f);
}

TEST(ChromeRasterButtonTest, NonPositiveSizeProducesAnEmptyImage) {
    EXPECT_TRUE(chrome_raster::render_button(plain_button(0, 10, 2.0f, 1.0f)).pixels.empty());
    EXPECT_TRUE(chrome_raster::render_button(plain_button(10, -1, 2.0f, 1.0f)).pixels.empty());
}

TEST(ChromeRasterButtonTest, ImageHasOnePixelPerCell) {
    const ChromeImage image = chrome_raster::render_button(plain_button(30, 12, 6.0f, 1.0f));
    EXPECT_EQ(image.width, 30);
    EXPECT_EQ(image.height, 12);
    EXPECT_EQ(image.pixels.size(), 360u);
}

TEST(ChromeRasterButtonTest, CenterIsFillAndEdgeMidpointIsBorder) {
    const ChromeImage image = chrome_raster::render_button(plain_button(26, 26, 6.0f, 1.0f));
    EXPECT_TRUE(same(image.at(13, 13), kFill));
    EXPECT_TRUE(same(image.at(13, 0), kBorder));
    EXPECT_TRUE(same(image.at(0, 13), kBorder));
}

TEST(ChromeRasterButtonTest, RoundedCornerPixelIsTransparent) {
    const ChromeImage image = chrome_raster::render_button(plain_button(26, 26, 6.0f, 1.0f));
    EXPECT_EQ(image.at(0, 0).a, 0);
    EXPECT_EQ(image.at(25, 25).a, 0);
}

TEST(ChromeRasterButtonTest, CornerEdgeIsAntialiasedRatherThanHard) {
    const ChromeImage image = chrome_raster::render_button(plain_button(26, 26, 6.0f, 1.0f));
    bool found_partial = false;
    for (int i = 0; i < 6; ++i) {
        const std::uint8_t a = image.at(i, 1).a;
        found_partial = found_partial || (a > 0 && a < 255);
    }
    EXPECT_TRUE(found_partial);
}

TEST(ChromeRasterButtonTest, VerticalGradientRunsFromTopToBottom) {
    ChromeButtonSpec spec = plain_button(20, 40, 4.0f, 0.0f);
    spec.fill_top = kWhite;
    spec.fill_bottom = kFillBottom;
    const ChromeImage image = chrome_raster::render_button(spec);
    EXPECT_GT(image.at(10, 2).r, image.at(10, 20).r);
    EXPECT_GT(image.at(10, 20).r, image.at(10, 37).r);
}

TEST(ChromeRasterButtonTest, TranslucentFillKeepsItsAlphaAndColor) {
    ChromeButtonSpec spec = plain_button(26, 26, 6.0f, 1.0f);
    spec.fill_top = ChromeColor{255, 255, 255, 10};
    spec.fill_bottom = spec.fill_top;
    const ChromeImage image = chrome_raster::render_button(spec);
    EXPECT_TRUE(same(image.at(13, 13), (ChromeColor{255, 255, 255, 10})));
}

TEST(ChromeRasterButtonTest, InsetHighlightBrightensOnlyTheTopRowsInsideTheBorder) {
    ChromeButtonSpec spec = plain_button(80, 30, 15.0f, 1.0f);
    spec.highlight_width = 1.0f;
    spec.highlight = ChromeColor{255, 255, 255, 128};
    const ChromeImage image = chrome_raster::render_button(spec);
    EXPECT_GT(image.at(40, 1).r, kFill.r);
    EXPECT_TRUE(same(image.at(40, 3), kFill));
    EXPECT_TRUE(same(image.at(40, 28), kFill));
}

TEST(ChromeRasterButtonTest, IconStrokesAreDrawnInTheIconColorAndLeaveTheCenterClear) {
    ChromeButtonSpec spec = plain_button(26, 26, 6.0f, 1.0f);
    spec.icon = ChromeIcon{chrome_raster::expand_icon(), ChromePoint{1.0f, 1.0f}, 24.0f, 2.0f, kWhite};
    const ChromeImage image = chrome_raster::render_button(spec);
    EXPECT_TRUE(same(image.at(1 + 6, 1 + 3), kWhite));
    EXPECT_TRUE(same(image.at(13, 13), kFill));
}

TEST(ChromeRasterButtonTest, ZeroSizeIconIsIgnoredInsteadOfDividingByZero) {
    ChromeButtonSpec spec = plain_button(26, 26, 6.0f, 1.0f);
    spec.icon = ChromeIcon{chrome_raster::expand_icon(), ChromePoint{0.0f, 0.0f}, 0.0f, 2.0f, kWhite};
    const ChromeImage image = chrome_raster::render_button(spec);
    EXPECT_TRUE(same(image.at(13, 13), kFill));
}

TEST(ChromeRasterButtonTest, BorderWiderThanTheButtonLeavesOnlyBorder) {
    const ChromeImage image = chrome_raster::render_button(plain_button(6, 6, 0.0f, 10.0f));
    EXPECT_TRUE(same(image.at(3, 3), kBorder));
}

TEST(ChromeRasterIconTest, IconsStayInsideTheViewBoxAndAreFourMirroredCorners) {
    for (const auto& icon : {chrome_raster::expand_icon(), chrome_raster::collapse_icon()}) {
        ASSERT_FALSE(icon.empty());
        EXPECT_EQ(icon.size() % 4, 0u);
        for (const ChromeSegment& s : icon) {
            for (const ChromePoint& p : {s.from, s.to}) {
                EXPECT_GE(p.x, 0.0f);
                EXPECT_LE(p.x, chrome_raster::kIconViewBox);
                EXPECT_GE(p.y, 0.0f);
                EXPECT_LE(p.y, chrome_raster::kIconViewBox);
            }
        }
    }
}

TEST(ChromeRasterIconTest, ExpandBracketsHugTheOuterCornersAndCollapseBracketsPointInward) {
    const ChromeSegment expand_first = chrome_raster::expand_icon().front();
    EXPECT_FLOAT_EQ(expand_first.from.x, 8.0f);
    EXPECT_FLOAT_EQ(expand_first.to.x, 5.0f);
    EXPECT_FLOAT_EQ(expand_first.to.y, 3.0f);
    const ChromeSegment collapse_first = chrome_raster::collapse_icon().front();
    EXPECT_FLOAT_EQ(collapse_first.to.x, 8.0f);
    EXPECT_FLOAT_EQ(collapse_first.to.y, 6.0f);
}
