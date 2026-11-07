#include "ui_host/panel_window_request.h"

#include <array>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace panel_window_request {

namespace {

constexpr std::array<std::pair<std::string_view, PanelWindowDragRegion>, 9> kRegionNames{{
    {"caption", PanelWindowDragRegion::Caption},
    {"top", PanelWindowDragRegion::Top},
    {"bottom", PanelWindowDragRegion::Bottom},
    {"left", PanelWindowDragRegion::Left},
    {"right", PanelWindowDragRegion::Right},
    {"top_left", PanelWindowDragRegion::TopLeft},
    {"top_right", PanelWindowDragRegion::TopRight},
    {"bottom_left", PanelWindowDragRegion::BottomLeft},
    {"bottom_right", PanelWindowDragRegion::BottomRight},
}};

constexpr std::array<std::pair<std::string_view, PanelWindowCommand>, 2> kCommandNames{{
    {"close", PanelWindowCommand::Close},
    {"minimize", PanelWindowCommand::Minimize},
}};

template <typename Value, std::size_t Count>
std::optional<Value> lookup(const std::array<std::pair<std::string_view, Value>, Count>& names,
                            const std::string& name) {
    for (const auto& [candidate, value] : names) {
        if (name == candidate) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<std::string> string_field_of_type(std::string_view json_text, std::string_view expected_type,
                                                const char* field_name) {
    nlohmann::json doc = nlohmann::json::parse(json_text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }
    const auto type = doc.find("type");
    if (type == doc.end() || !type->is_string() || type->get<std::string>() != expected_type) {
        return std::nullopt;
    }
    const auto field = doc.find(field_name);
    if (field == doc.end() || !field->is_string()) {
        return std::nullopt;
    }
    return field->get<std::string>();
}

}

std::optional<PanelWindowDragRegion> parse_drag(std::string_view json_text) {
    const std::optional<std::string> region = string_field_of_type(json_text, kWindowDragType, "region");
    if (!region.has_value()) {
        return std::nullopt;
    }
    return lookup(kRegionNames, *region);
}

std::optional<PanelWindowCommand> parse_command(std::string_view json_text) {
    const std::optional<std::string> command = string_field_of_type(json_text, kWindowCommandType, "command");
    if (!command.has_value()) {
        return std::nullopt;
    }
    return lookup(kCommandNames, *command);
}

}
