#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "visuals/palette.h"

namespace palette_fixtures {

inline Palette warm_cool() {
    return Palette{{{0.0f, Color{40, 90, 255, 255}}, {1.0f, Color{255, 60, 180, 255}}}};
}

inline Palette sunset() {
    return Palette{
        {{0.0f, Color{255, 220, 0, 255}}, {0.5f, Color{255, 80, 0, 255}}, {1.0f, Color{120, 0, 200, 255}}}};
}

inline Palette fractional_position_odd_alpha() {
    return Palette{
        {{0.1f, Color{1, 2, 3, 4}}, {0.333333f, Color{250, 251, 252, 0}}, {0.7f, Color{9, 8, 7, 128}}}};
}

inline std::vector<std::optional<Palette>> all_cases() {
    return {warm_cool(), sunset(), fractional_position_odd_alpha(), std::nullopt};
}

inline void expect_same_stops(const std::optional<Palette>& actual, const std::optional<Palette>& expected) {
    ASSERT_EQ(actual.has_value(), expected.has_value());
    if (!expected.has_value()) {
        return;
    }
    ASSERT_EQ(actual->stops.size(), expected->stops.size());
    for (std::size_t i = 0; i < expected->stops.size(); ++i) {
        EXPECT_EQ(actual->stops[i].position, expected->stops[i].position) << "stop " << i;
        EXPECT_EQ(actual->stops[i].color.r, expected->stops[i].color.r) << "stop " << i;
        EXPECT_EQ(actual->stops[i].color.g, expected->stops[i].color.g) << "stop " << i;
        EXPECT_EQ(actual->stops[i].color.b, expected->stops[i].color.b) << "stop " << i;
        EXPECT_EQ(actual->stops[i].color.a, expected->stops[i].color.a) << "stop " << i;
    }
}

}
