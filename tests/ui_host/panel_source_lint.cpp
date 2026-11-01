#include "ui_host/panel_source_lint.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>

namespace {

constexpr std::array<std::string_view, 148> kNamedColors{
    "aliceblue", "antiquewhite", "aqua", "aquamarine", "azure", "beige", "bisque", "black", "blanchedalmond",
    "blue", "blueviolet", "brown", "burlywood", "cadetblue", "chartreuse", "chocolate", "coral", "cornflowerblue",
    "cornsilk", "crimson", "cyan", "darkblue", "darkcyan", "darkgoldenrod", "darkgray", "darkgreen", "darkgrey",
    "darkkhaki", "darkmagenta", "darkolivegreen", "darkorange", "darkorchid", "darkred", "darksalmon",
    "darkseagreen", "darkslateblue", "darkslategray", "darkslategrey", "darkturquoise", "darkviolet", "deeppink",
    "deepskyblue", "dimgray", "dimgrey", "dodgerblue", "firebrick", "floralwhite", "forestgreen", "fuchsia",
    "gainsboro", "ghostwhite", "gold", "goldenrod", "gray", "green", "greenyellow", "grey", "honeydew", "hotpink",
    "indianred", "indigo", "ivory", "khaki", "lavender", "lavenderblush", "lawngreen", "lemonchiffon", "lightblue",
    "lightcoral", "lightcyan", "lightgoldenrodyellow", "lightgray", "lightgreen", "lightgrey", "lightpink",
    "lightsalmon", "lightseagreen", "lightskyblue", "lightslategray", "lightslategrey", "lightsteelblue",
    "lightyellow", "lime", "limegreen", "linen", "magenta", "maroon", "mediumaquamarine", "mediumblue",
    "mediumorchid", "mediumpurple", "mediumseagreen", "mediumslateblue", "mediumspringgreen", "mediumturquoise",
    "mediumvioletred", "midnightblue", "mintcream", "mistyrose", "moccasin", "navajowhite", "navy", "oldlace",
    "olive", "olivedrab", "orange", "orangered", "orchid", "palegoldenrod", "palegreen", "paleturquoise",
    "palevioletred", "papayawhip", "peachpuff", "peru", "pink", "plum", "powderblue", "purple", "rebeccapurple",
    "red", "rosybrown", "royalblue", "saddlebrown", "salmon", "sandybrown", "seagreen", "seashell", "sienna",
    "silver", "skyblue", "slateblue", "slategray", "slategrey", "snow", "springgreen", "steelblue", "tan", "teal",
    "thistle", "tomato", "turquoise", "violet", "wheat", "white", "whitesmoke", "yellow", "yellowgreen"};

constexpr std::array<std::string_view, 10> kColorFunctions{"rgb", "rgba", "hsl", "hsla", "hwb",
                                                           "lab", "lch", "oklab", "oklch", "color"};

bool is_token_character(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '-' || c == '_';
}

template <std::size_t N>
bool contains(const std::array<std::string_view, N>& list, std::string_view value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

std::string lowercased(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::vector<std::string> declaration_values(std::string_view css) {
    std::vector<std::string> values;
    std::size_t segment_begin = 0;
    for (std::size_t i = 0; i <= css.size(); ++i) {
        const char c = i < css.size() ? css[i] : ';';
        if (c != '{' && c != '}' && c != ';') {
            continue;
        }
        const std::string_view segment = css.substr(segment_begin, i - segment_begin);
        segment_begin = i + 1;
        if (c == '{') {
            continue;
        }
        const std::size_t colon = segment.find(':');
        if (colon != std::string_view::npos) {
            values.push_back(lowercased(segment.substr(colon + 1)));
        }
    }
    return values;
}

void collect_color_literals(const std::string& value, std::vector<std::string>& found) {
    for (std::size_t i = 0; i < value.size();) {
        if (value[i] == '#') {
            std::size_t end = i + 1;
            while (end < value.size() && std::isxdigit(static_cast<unsigned char>(value[end])) != 0) {
                ++end;
            }
            if (end > i + 1) {
                found.push_back(value.substr(i, end - i));
            }
            i = end;
            continue;
        }
        if (!is_token_character(value[i])) {
            ++i;
            continue;
        }
        std::size_t end = i;
        while (end < value.size() && is_token_character(value[end])) {
            ++end;
        }
        const std::string token = value.substr(i, end - i);
        const bool is_call = end < value.size() && value[end] == '(';
        if ((is_call && contains(kColorFunctions, token)) || (!is_call && contains(kNamedColors, token))) {
            found.push_back(token);
        }
        i = end;
    }
}

bool is_space(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::size_t skip_spaces(std::string_view text, std::size_t position) {
    while (position < text.size() && is_space(text[position])) {
        ++position;
    }
    return position;
}

bool has_set_attribute_style(std::string_view js) {
    constexpr std::string_view kCall = "setAttribute";
    constexpr std::string_view kStyle = "style";
    for (std::size_t at = js.find(kCall); at != std::string_view::npos; at = js.find(kCall, at + 1)) {
        std::size_t position = skip_spaces(js, at + kCall.size());
        if (position >= js.size() || js[position] != '(') {
            continue;
        }
        position = skip_spaces(js, position + 1);
        if (position >= js.size() || (js[position] != '"' && js[position] != '\'' && js[position] != '`')) {
            continue;
        }
        const char quote = js[position];
        if (js.substr(position + 1, kStyle.size()) == kStyle && position + 1 + kStyle.size() < js.size() &&
            js[position + 1 + kStyle.size()] == quote) {
            return true;
        }
    }
    return false;
}

}

namespace panel_source_lint {

std::vector<std::string> css_color_literals(std::string_view css) {
    std::vector<std::string> found;
    for (const std::string& value : declaration_values(css)) {
        collect_color_literals(value, found);
    }
    return found;
}

std::vector<std::string> js_forbidden_patterns(std::string_view js) {
    std::vector<std::string> found;
    for (std::string_view pattern : {"innerHTML", "outerHTML", "insertAdjacentHTML", "document.write"}) {
        if (js.find(pattern) != std::string_view::npos) {
            found.emplace_back(pattern);
        }
    }
    if (has_set_attribute_style(js)) {
        found.emplace_back("setAttribute(style)");
    }
    return found;
}

std::vector<std::string> css_custom_properties_used(std::string_view css) {
    constexpr std::string_view kPrefix = "var(";
    std::vector<std::string> names;
    for (std::size_t at = css.find(kPrefix); at != std::string_view::npos; at = css.find(kPrefix, at + 1)) {
        const std::size_t begin = skip_spaces(css, at + kPrefix.size());
        std::size_t end = begin;
        while (end < css.size() && is_token_character(css[end])) {
            ++end;
        }
        names.emplace_back(css.substr(begin, end - begin));
    }
    return names;
}

}
