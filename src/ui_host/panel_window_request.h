#pragma once

#include <optional>
#include <string_view>

enum class PanelWindowDragRegion {
    Caption,
    Top,
    Bottom,
    Left,
    Right,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

enum class PanelWindowCommand {
    Close,
    Minimize,
};

namespace panel_window_request {

inline constexpr char kWindowDragType[] = "window_drag";
inline constexpr char kWindowCommandType[] = "window_command";

std::optional<PanelWindowDragRegion> parse_drag(std::string_view json_text);
std::optional<PanelWindowCommand> parse_command(std::string_view json_text);

}
