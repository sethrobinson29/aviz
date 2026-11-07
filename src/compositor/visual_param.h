#pragma once

#include <string>

#include "compositor/layer.h"
#include "compositor/visual_param_value.h"

namespace visual_param {

bool set(Layer& layer, const std::string& param_name, const VisualParamValue& value);

}
