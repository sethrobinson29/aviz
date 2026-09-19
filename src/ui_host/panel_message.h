#pragma once

#include <optional>
#include <string_view>

enum class PanelMessageType {
    AddLayer,
};

struct PanelMessage {
    PanelMessageType type;
};

namespace panel_message {

std::optional<PanelMessage> parse(std::string_view json_text);

}
