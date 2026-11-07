#pragma once

#include <string>

#include "app/audio_source_controller.h"
#include "app/window_layout_controller.h"
#include "compositor/compositor.h"
#include "ui_host/panel_message.h"
#include "ui_host/ui_host.h"

namespace panel_message_apply {

struct PanelApplyContext {
    Compositor& compositor;
    UIHost& ui_host;
    WindowLayoutController& window_layout;
    AudioSourceController& audio_source;
    std::string presets_dir;
};

void apply(const PanelMessage& message, PanelApplyContext& ctx);

void push_window_state(UIHost& ui_host, const WindowLayoutController& window_layout,
                        const AudioSourceController& audio_source);

}
