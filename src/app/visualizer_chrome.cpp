#include "app/visualizer_chrome.h"

#include <cmath>
#include <cstdio>
#include <string>

#include "app/chrome_raster.h"

namespace {

static_assert(sizeof(ChromeColor) == 4, "ChromeColor must match raylib's R8G8B8A8 pixel layout");

constexpr std::size_t kNormal = 0;
constexpr std::size_t kHover = 1;
constexpr float kLayoutProbeScreenSize = 4096.0f;

Color to_raylib(ChromeColor c) { return Color{c.r, c.g, c.b, c.a}; }

std::string resolve_font_path(const char* relative) {
    std::string candidate = std::string(GetApplicationDirectory()) + relative;
    if (FileExists(candidate.c_str())) {
        return candidate;
    }
    return relative;
}

Texture2D upload(ChromeImage image) {
    if (image.pixels.empty()) {
        return Texture2D{};
    }
    Image raylib_image{image.pixels.data(), image.width, image.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    return LoadTextureFromImage(raylib_image);
}

void unload(Texture2D& texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
    }
    texture = Texture2D{};
}

ChromeButtonSpec toggle_spec(const FullscreenToggleTheme& toggle, const OverlayLayout& layout, float scale,
                             bool hovered) {
    const ChromeColor fill = hovered ? toggle.fill_hover : toggle.fill;
    return ChromeButtonSpec{
        static_cast<int>(layout.rect.width),
        static_cast<int>(layout.rect.height),
        toggle.corner_radius * scale,
        toggle.border_width * scale,
        0.0f,
        fill,
        fill,
        hovered ? toggle.border_hover : toggle.border,
        ChromeColor{0, 0, 0, 0},
        ChromeIcon{chrome_raster::expand_icon(),
                   ChromePoint{layout.icon_in_button.x, layout.icon_in_button.y},
                   layout.icon_in_button.width,
                   toggle.icon_stroke_width,
                   hovered ? toggle.icon_hover : toggle.icon},
    };
}

ChromeButtonSpec pill_spec(const ExitFullscreenPillTheme& pill, const OverlayLayout& layout, float scale,
                           bool hovered) {
    return ChromeButtonSpec{
        static_cast<int>(layout.rect.width),
        static_cast<int>(layout.rect.height),
        layout.rect.height * 0.5f,
        pill.border_width * scale,
        pill.highlight_width * scale,
        pill.fill_top,
        pill.fill_bottom,
        hovered ? pill.border_hover : pill.border,
        pill.highlight,
        ChromeIcon{chrome_raster::collapse_icon(),
                   ChromePoint{layout.icon_in_button.x, layout.icon_in_button.y},
                   layout.icon_in_button.width,
                   pill.icon_stroke_width,
                   pill.content},
    };
}

}

void draw_visualizer_background(const VisualizerChromeTheme& theme) { ClearBackground(to_raylib(theme.background)); }

VisualizerChrome::VisualizerChrome(const VisualizerChromeTheme& theme) : theme_(theme) {
    rebuild(GetWindowScaleDPI().x);
}

VisualizerChrome::~VisualizerChrome() {
    unload_textures();
    unload_font();
}

float VisualizerChrome::font_pixel_size() const { return std::round(theme_.exit_pill.font_size * scale_); }

float VisualizerChrome::label_spacing() const { return theme_.exit_pill.letter_spacing * scale_; }

Vector2 VisualizerChrome::measure_label() const {
    return MeasureTextEx(font_, theme_.exit_label, font_pixel_size(), label_spacing());
}

void VisualizerChrome::load_font(int pixel_size) {
    unload_font();
    const std::string path = resolve_font_path(theme_.font_file);
    Font loaded = LoadFontEx(path.c_str(), pixel_size, nullptr, 0);
    if (IsFontValid(loaded) && loaded.texture.id != 0) {
        font_ = loaded;
        font_owned_ = true;
        return;
    }
    if (loaded.texture.id != 0) {
        UnloadFont(loaded);
    }
    std::fprintf(stderr, "visualizer: could not load chrome font %s, using raylib's default font\n", path.c_str());
    font_ = GetFontDefault();
    font_owned_ = false;
}

void VisualizerChrome::unload_font() {
    if (font_owned_) {
        UnloadFont(font_);
    }
    font_ = Font{};
    font_owned_ = false;
}

void VisualizerChrome::unload_textures() {
    for (Texture2D& texture : toggle_textures_) {
        unload(texture);
    }
    for (Texture2D& texture : pill_textures_) {
        unload(texture);
    }
}

void VisualizerChrome::rebuild(float scale) {
    scale_ = scale > 0.0f ? scale : 1.0f;
    load_font(static_cast<int>(font_pixel_size()));
    unload_textures();

    const Vector2 label = measure_label();
    const OverlayLayout toggle = visualizer_overlay::layout(theme_, false, kLayoutProbeScreenSize,
                                                            kLayoutProbeScreenSize, scale_, label.x, label.y);
    const OverlayLayout pill = visualizer_overlay::layout(theme_, true, kLayoutProbeScreenSize,
                                                          kLayoutProbeScreenSize, scale_, label.x, label.y);
    for (std::size_t variant : {kNormal, kHover}) {
        const bool hovered = variant == kHover;
        toggle_textures_[variant] = upload(chrome_raster::render_button(
            toggle_spec(theme_.fullscreen_toggle, toggle, scale_, hovered)));
        pill_textures_[variant] =
            upload(chrome_raster::render_button(pill_spec(theme_.exit_pill, pill, scale_, hovered)));
    }
}

bool VisualizerChrome::update(bool fullscreen) {
    const float scale = GetWindowScaleDPI().x;
    if (scale > 0.0f && scale != scale_) {
        rebuild(scale);
    }

    const Vector2 label = measure_label();
    const OverlayButton previous_button = layout_.button;
    layout_ = visualizer_overlay::layout(theme_, fullscreen, static_cast<float>(GetScreenWidth()),
                                         static_cast<float>(GetScreenHeight()), scale_, label.x, label.y);
    if (layout_.button != previous_button) {
        state_ = OverlayButtonState{};
    }

    const Vector2 mouse = GetMousePosition();
    const OverlayPointer pointer{IsCursorOnScreen(), mouse.x, mouse.y, IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                 IsMouseButtonReleased(MOUSE_BUTTON_LEFT)};
    const OverlayStep step = visualizer_overlay::step(state_, layout_.rect, pointer);
    state_ = step.state;
    return step.activated;
}

void VisualizerChrome::draw() const {
    const std::size_t variant = state_.hovered ? kHover : kNormal;
    const bool pill = layout_.button == OverlayButton::ExitFullscreen;
    const Texture2D& texture = pill ? pill_textures_[variant] : toggle_textures_[variant];
    if (texture.id != 0) {
        DrawTexture(texture, static_cast<int>(layout_.rect.x), static_cast<int>(layout_.rect.y), WHITE);
    }
    if (pill) {
        DrawTextEx(font_, theme_.exit_label, Vector2{layout_.label_x, layout_.label_y}, font_pixel_size(),
                   label_spacing(), to_raylib(theme_.exit_pill.content));
    }
}

bool VisualizerChrome::is_over_button(int client_x, int client_y) const {
    return layout_.rect.contains(static_cast<float>(client_x), static_cast<float>(client_y));
}
