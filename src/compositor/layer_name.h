#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace layer_name {

inline constexpr std::size_t kMaxCodePoints = 50;

std::optional<std::string> normalized(std::string_view utf8_text);

}
