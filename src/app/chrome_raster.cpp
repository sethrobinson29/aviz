#include "app/chrome_raster.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace chrome_raster {

namespace {

constexpr int kArcSegments = 6;
constexpr float kPi = 3.14159265358979f;

struct Premultiplied {
    float r;
    float g;
    float b;
    float a;
};

Premultiplied premultiply(ChromeColor c) {
    const float a = static_cast<float>(c.a) / 255.0f;
    return Premultiplied{static_cast<float>(c.r) / 255.0f * a, static_cast<float>(c.g) / 255.0f * a,
                         static_cast<float>(c.b) / 255.0f * a, a};
}

Premultiplied scaled(Premultiplied c, float k) { return Premultiplied{c.r * k, c.g * k, c.b * k, c.a * k}; }

Premultiplied over(Premultiplied src, Premultiplied dst) {
    const float keep = 1.0f - src.a;
    return Premultiplied{src.r + dst.r * keep, src.g + dst.g * keep, src.b + dst.b * keep, src.a + dst.a * keep};
}

Premultiplied mix(Premultiplied a, Premultiplied b, float t) {
    return Premultiplied{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                         a.a + (b.a - a.a) * t};
}

std::uint8_t to_byte(float unit) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(unit, 0.0f, 1.0f) * 255.0f));
}

ChromeColor unpremultiply(Premultiplied c) {
    const float a = std::clamp(c.a, 0.0f, 1.0f);
    if (a <= 0.0f) {
        return ChromeColor{0, 0, 0, 0};
    }
    return ChromeColor{to_byte(c.r / a), to_byte(c.g / a), to_byte(c.b / a), to_byte(a)};
}

float coverage(float distance) { return std::clamp(0.5f - distance, 0.0f, 1.0f); }

void append_polyline(std::vector<ChromeSegment>& out, const std::vector<ChromePoint>& points) {
    for (std::size_t i = 1; i < points.size(); ++i) {
        out.push_back(ChromeSegment{points[i - 1], points[i]});
    }
}

void append_arc(std::vector<ChromePoint>& points, ChromePoint center, float radius, float from_degrees,
                float to_degrees) {
    for (int step = 1; step <= kArcSegments; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(kArcSegments);
        const float radians = (from_degrees + (to_degrees - from_degrees) * t) * kPi / 180.0f;
        points.push_back(ChromePoint{center.x + radius * std::cos(radians), center.y + radius * std::sin(radians)});
    }
}

std::vector<ChromeSegment> mirrored_into_four_corners(const std::vector<ChromePoint>& top_left_corner) {
    std::vector<ChromeSegment> segments;
    const std::array<std::array<bool, 2>, 4> mirrors{{{false, false}, {true, false}, {false, true}, {true, true}}};
    for (const auto& mirror : mirrors) {
        std::vector<ChromePoint> corner;
        corner.reserve(top_left_corner.size());
        for (const ChromePoint& p : top_left_corner) {
            corner.push_back(ChromePoint{mirror[0] ? kIconViewBox - p.x : p.x, mirror[1] ? kIconViewBox - p.y : p.y});
        }
        append_polyline(segments, corner);
    }
    return segments;
}

}

std::vector<ChromeSegment> expand_icon() {
    std::vector<ChromePoint> corner{{8.0f, 3.0f}, {5.0f, 3.0f}};
    append_arc(corner, ChromePoint{5.0f, 5.0f}, 2.0f, -90.0f, -180.0f);
    corner.push_back(ChromePoint{3.0f, 8.0f});
    return mirrored_into_four_corners(corner);
}

std::vector<ChromeSegment> collapse_icon() {
    std::vector<ChromePoint> corner{{8.0f, 3.0f}, {8.0f, 6.0f}};
    append_arc(corner, ChromePoint{6.0f, 6.0f}, 2.0f, 0.0f, 90.0f);
    corner.push_back(ChromePoint{3.0f, 8.0f});
    return mirrored_into_four_corners(corner);
}

float rounded_rect_distance(ChromePoint p, float width, float height, float corner_radius) {
    const float half_w = width * 0.5f;
    const float half_h = height * 0.5f;
    const float radius = std::clamp(corner_radius, 0.0f, std::min(half_w, half_h));
    const float qx = std::fabs(p.x - half_w) - (half_w - radius);
    const float qy = std::fabs(p.y - half_h) - (half_h - radius);
    const float outside = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f));
    const float inside = std::min(std::max(qx, qy), 0.0f);
    return outside + inside - radius;
}

float segment_distance(ChromePoint p, const ChromeSegment& segment) {
    const float dx = segment.to.x - segment.from.x;
    const float dy = segment.to.y - segment.from.y;
    const float length_squared = dx * dx + dy * dy;
    float t = 0.0f;
    if (length_squared > 0.0f) {
        t = std::clamp(((p.x - segment.from.x) * dx + (p.y - segment.from.y) * dy) / length_squared, 0.0f, 1.0f);
    }
    return std::hypot(p.x - (segment.from.x + dx * t), p.y - (segment.from.y + dy * t));
}

ChromeImage render_button(const ChromeButtonSpec& spec) {
    ChromeImage image;
    if (spec.width <= 0 || spec.height <= 0) {
        return image;
    }
    image.width = spec.width;
    image.height = spec.height;
    image.pixels.resize(static_cast<std::size_t>(spec.width) * static_cast<std::size_t>(spec.height));

    const float width = static_cast<float>(spec.width);
    const float height = static_cast<float>(spec.height);
    const float border = std::max(spec.border_width, 0.0f);
    const float inner_width = std::max(width - 2.0f * border, 0.0f);
    const float inner_height = std::max(height - 2.0f * border, 0.0f);
    const float inner_radius = std::max(spec.corner_radius - border, 0.0f);
    const float icon_scale = spec.icon.size / kIconViewBox;
    const float icon_half_stroke = spec.icon.stroke_width * icon_scale * 0.5f;

    const Premultiplied fill_top = premultiply(spec.fill_top);
    const Premultiplied fill_bottom = premultiply(spec.fill_bottom);
    const Premultiplied border_color = premultiply(spec.border);
    const Premultiplied highlight = premultiply(spec.highlight);
    const Premultiplied icon_color = premultiply(spec.icon.color);

    auto inner_coverage = [&](ChromePoint p) {
        if (inner_width <= 0.0f || inner_height <= 0.0f) {
            return 0.0f;
        }
        return coverage(rounded_rect_distance(ChromePoint{p.x - border, p.y - border}, inner_width, inner_height,
                                              inner_radius));
    };

    for (int y = 0; y < spec.height; ++y) {
        for (int x = 0; x < spec.width; ++x) {
            const ChromePoint p{static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
            const float outer = coverage(rounded_rect_distance(p, width, height, spec.corner_radius));
            const float inner = inner_coverage(p);

            Premultiplied content = mix(fill_top, fill_bottom, p.y / height);
            if (spec.highlight_width > 0.0f) {
                const float shifted = inner_coverage(ChromePoint{p.x, p.y - spec.highlight_width});
                content = over(scaled(highlight, inner * (1.0f - shifted)), content);
            }
            if (!spec.icon.segments.empty() && icon_scale > 0.0f) {
                float nearest = std::numeric_limits<float>::max();
                const ChromePoint icon_space{(p.x - spec.icon.origin.x) / icon_scale,
                                             (p.y - spec.icon.origin.y) / icon_scale};
                for (const ChromeSegment& segment : spec.icon.segments) {
                    nearest = std::min(nearest, segment_distance(icon_space, segment));
                }
                content = over(scaled(icon_color, coverage(nearest * icon_scale - icon_half_stroke)), content);
            }

            const Premultiplied shaded = mix(border_color, content, inner);
            image.pixels[static_cast<std::size_t>(y * spec.width + x)] = unpremultiply(scaled(shaded, outer));
        }
    }
    return image;
}

}
