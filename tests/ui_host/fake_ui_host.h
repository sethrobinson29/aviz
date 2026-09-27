#pragma once

#include <optional>
#include <string>
#include <vector>

#include "app/window_layout_config.h"
#include "ui_host/panel_message.h"
#include "ui_host/ui_host.h"

class FakeUIHost : public UIHost {
public:
    void show() override { visible_ = true; }
    void hide() override { visible_ = false; }
    bool is_visible() const override { return visible_; }
    bool should_close() const override { return should_close_; }
    void set_bounds(int x, int y, int width, int height) override {
        last_bounds = WindowRect{x, y, width, height};
    }
    std::vector<PanelMessage> drain_pending_messages() override { return {}; }

    void send_to_panel(const std::string& json_text) override { sent_messages.push_back(json_text); }

    std::vector<std::string> sent_messages;
    std::optional<WindowRect> last_bounds;
    bool should_close_ = false;

private:
    bool visible_ = true;
};
