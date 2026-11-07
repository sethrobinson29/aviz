#include "ui_host/panel_dispatch_queue.h"

void PanelDispatchQueue::push(PanelMessage message) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(message);
}

std::vector<PanelMessage> PanelDispatchQueue::drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<PanelMessage> drained(queue_.begin(), queue_.end());
    queue_.clear();
    return drained;
}
