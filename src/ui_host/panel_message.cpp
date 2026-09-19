#include "ui_host/panel_message.h"

#include <string>

#include <nlohmann/json.hpp>

namespace panel_message {

std::optional<PanelMessage> parse(std::string_view json_text) {
    try {
        const nlohmann::json doc = nlohmann::json::parse(json_text);
        if (!doc.is_object()) {
            return std::nullopt;
        }
        const std::string type = doc.at("type").get<std::string>();
        if (type == "add_layer") {
            return PanelMessage{PanelMessageType::AddLayer};
        }
        return std::nullopt;
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

}
