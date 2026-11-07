#pragma once

#include <vector>

#include "app/visualizer_chrome_theme.h"

struct ChromePoint {
    float x;
    float y;
};

struct ChromeSegment {
    ChromePoint from;
    ChromePoint to;
};

struct ChromeImage {
    int width = 0;
    int height = 0;
    std::vector<ChromeColor> pixels;

    ChromeColor at(int x, int y) const { return pixels[static_cast<std::size_t>(y * width + x)]; }
};

struct ChromeIcon {
    std::vector<ChromeSegment> segments;
    ChromePoint origin;
    float size;
    float stroke_width;
    ChromeColor color;
};

struct ChromeButtonSpec {
    int width;
    int height;
    float corner_radius;
    float border_width;
    float highlight_width;
    ChromeColor fill_top;
    ChromeColor fill_bottom;
    ChromeColor border;
    ChromeColor highlight;
    ChromeIcon icon;
};

namespace chrome_raster {

inline constexpr float kIconViewBox = 24.0f;

std::vector<ChromeSegment> expand_icon();
std::vector<ChromeSegment> collapse_icon();

float rounded_rect_distance(ChromePoint p, float width, float height, float corner_radius);
float segment_distance(ChromePoint p, const ChromeSegment& segment);

ChromeImage render_button(const ChromeButtonSpec& spec);

}
