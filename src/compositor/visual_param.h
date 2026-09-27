#pragma once

#include <string>

#include "compositor/layer.h"

namespace visual_param {

bool set(Layer& layer, const std::string& param_name, float value);

}
