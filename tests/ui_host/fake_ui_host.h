#pragma once

#include <string>
#include <vector>

#include "ui_host/panel_message.h"
#include "ui_host/ui_host.h"

class FakeUIHost : public UIHost {
public:
    void show() override { visible_ = true; }
    void hide() override { visible_ = false; }
    bool is_visible() const override { return visible_; }
    std::vector<PanelMessage> drain_pending_messages() override { return {}; }

    void send_to_panel(const std::string& json_text) override { sent_messages.push_back(json_text); }

    std::vector<std::string> sent_messages;

private:
    bool visible_ = true;
};
