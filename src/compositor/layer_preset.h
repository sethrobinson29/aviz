#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "compositor/layer.h"
#include "visuals/palette.h"

namespace layer_preset {

std::string serialize(const std::vector<Layer>& layers);
std::optional<std::vector<Layer>> deserialize(const std::string& json_text);
Palette palette_from_json(const nlohmann::json& j);

}
