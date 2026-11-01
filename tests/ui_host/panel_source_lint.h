#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace panel_source_lint {

std::vector<std::string> css_color_literals(std::string_view css);
std::vector<std::string> js_forbidden_patterns(std::string_view js);
std::vector<std::string> css_custom_properties_used(std::string_view css);

}
