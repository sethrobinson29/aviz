#pragma once

#include "compositor/compositor.h"
#include "ui_host/panel_message.h"
#include "ui_host/ui_host.h"

namespace panel_message_apply {

void apply(const PanelMessage& message, Compositor& compositor, UIHost& ui_host);

}
