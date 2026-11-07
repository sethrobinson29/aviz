#pragma once

#include <cstdint>

struct ChromeColor {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
    std::uint8_t a;
};

struct FullscreenToggleTheme {
    float size;
    float margin;
    float corner_radius;
    float border_width;
    float icon_size;
    float icon_stroke_width;
    ChromeColor fill;
    ChromeColor fill_hover;
    ChromeColor border;
    ChromeColor border_hover;
    ChromeColor icon;
    ChromeColor icon_hover;
};

struct ExitFullscreenPillTheme {
    float margin;
    float padding_top;
    float padding_bottom;
    float padding_left;
    float padding_right;
    float gap;
    float border_width;
    float highlight_width;
    float icon_size;
    float icon_stroke_width;
    float font_size;
    float letter_spacing;
    ChromeColor fill_top;
    ChromeColor fill_bottom;
    ChromeColor border;
    ChromeColor border_hover;
    ChromeColor highlight;
    ChromeColor content;
};

struct VisualizerChromeTheme {
    ChromeColor background;
    FullscreenToggleTheme fullscreen_toggle;
    ExitFullscreenPillTheme exit_pill;
    const char* font_file;
    const char* exit_label;
};

namespace visualizer_chrome_theme {

inline constexpr ChromeColor kPanelButtonGradientTop{0x2a, 0x31, 0x3b, 0xff};
inline constexpr ChromeColor kPanelButtonGradientBottom{0x1b, 0x20, 0x26, 0xff};
inline constexpr ChromeColor kPanelButtonText{0xcd, 0xd8, 0xde, 0xff};
inline constexpr ChromeColor kPanelButtonBorderHover{0x45, 0x4e, 0x59, 0xff};

inline constexpr VisualizerChromeTheme kDarkY2k{
    ChromeColor{0x00, 0x00, 0x00, 0xff},
    FullscreenToggleTheme{
        26.0f,
        16.0f,
        6.0f,
        1.0f,
        13.0f,
        2.0f,
        ChromeColor{0xff, 0xff, 0xff, 0x0a},
        ChromeColor{0xff, 0xff, 0xff, 0x17},
        ChromeColor{0xff, 0xff, 0xff, 0x14},
        ChromeColor{0xff, 0xff, 0xff, 0x29},
        ChromeColor{0x5c, 0x6b, 0x61, 0xff},
        ChromeColor{0x9a, 0xa6, 0x9e, 0xff},
    },
    ExitFullscreenPillTheme{
        28.0f,
        10.0f,
        10.0f,
        14.0f,
        18.0f,
        8.0f,
        1.0f,
        1.0f,
        14.0f,
        2.0f,
        12.0f,
        0.3f,
        kPanelButtonGradientTop,
        kPanelButtonGradientBottom,
        ChromeColor{0x3a, 0x42, 0x4d, 0xff},
        kPanelButtonBorderHover,
        ChromeColor{0xff, 0xff, 0xff, 0x2e},
        kPanelButtonText,
    },
    "assets/fonts/Inter-SemiBold-ascii.ttf",
    "Exit Fullscreen",
};

}
