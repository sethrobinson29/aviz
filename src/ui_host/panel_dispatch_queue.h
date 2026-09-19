#pragma once

#include <deque>
#include <mutex>
#include <vector>

#include "ui_host/panel_message.h"

class PanelDispatchQueue {
public:
    void push(PanelMessage message);
    std::vector<PanelMessage> drain();

private:
    std::mutex mutex_;
    std::deque<PanelMessage> queue_;
};
