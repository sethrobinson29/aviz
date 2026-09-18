#pragma once

#include <optional>
#include <string>
#include <vector>

#include "compositor/layer.h"

namespace layer_preset {

std::string serialize(const std::vector<Layer>& layers);
std::optional<std::vector<Layer>> deserialize(const std::string& json_text);

}
