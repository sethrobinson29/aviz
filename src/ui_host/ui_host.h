#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ui_host/panel_message.h"

class UIHost {
public:
    virtual ~UIHost() = default;

    virtual void show() = 0;
    virtual void hide() = 0;
    virtual bool is_visible() const = 0;
    virtual bool should_close() const = 0;
    virtual void set_bounds(int x, int y, int width, int height) = 0;
    virtual std::vector<PanelMessage> drain_pending_messages() = 0;
    virtual void send_to_panel(const std::string& json_text) = 0;
};

std::unique_ptr<UIHost> create_ui_host(const std::string& content_dir, int initial_x, int initial_y,
                                        int initial_width, int initial_height);
