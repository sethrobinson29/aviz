#pragma once

#include <array>

#include "app/visualizer_chrome_theme.h"
#include "app/visualizer_overlay.h"
#include "raylib.h"

void draw_visualizer_background(const VisualizerChromeTheme& theme);

class VisualizerChrome {
public:
    explicit VisualizerChrome(const VisualizerChromeTheme& theme);
    ~VisualizerChrome();

    VisualizerChrome(const VisualizerChrome&) = delete;
    VisualizerChrome& operator=(const VisualizerChrome&) = delete;

    bool update(bool fullscreen);
    void draw() const;
    bool is_over_button(int client_x, int client_y) const;

private:
    void rebuild(float scale);
    void load_font(int pixel_size);
    void unload_font();
    void unload_textures();
    float font_pixel_size() const;
    float label_spacing() const;
    Vector2 measure_label() const;

    const VisualizerChromeTheme& theme_;
    float scale_ = 0.0f;
    Font font_{};
    bool font_owned_ = false;
    std::array<Texture2D, 2> toggle_textures_{};
    std::array<Texture2D, 2> pill_textures_{};
    OverlayLayout layout_{};
    OverlayButtonState state_{};
};
