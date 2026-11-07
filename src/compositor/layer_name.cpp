#include "compositor/layer_name.h"

namespace {

struct DecodedCodePoint {
    char32_t value;
    std::size_t byte_length;
};

bool is_continuation_byte(unsigned char byte) { return (byte & 0xC0) == 0x80; }

std::optional<DecodedCodePoint> decode_at(std::string_view text, std::size_t position) {
    const auto lead = static_cast<unsigned char>(text[position]);
    if (lead < 0x80) {
        return DecodedCodePoint{lead, 1};
    }

    std::size_t byte_length = 0;
    char32_t value = 0;
    char32_t smallest_for_length = 0;
    if (lead >= 0xC2 && lead <= 0xDF) {
        byte_length = 2;
        value = lead & 0x1F;
        smallest_for_length = 0x80;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
        byte_length = 3;
        value = lead & 0x0F;
        smallest_for_length = 0x800;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
        byte_length = 4;
        value = lead & 0x07;
        smallest_for_length = 0x10000;
    } else {
        return std::nullopt;
    }

    if (position + byte_length > text.size()) {
        return std::nullopt;
    }
    for (std::size_t i = 1; i < byte_length; ++i) {
        const auto byte = static_cast<unsigned char>(text[position + i]);
        if (!is_continuation_byte(byte)) {
            return std::nullopt;
        }
        value = (value << 6) | (byte & 0x3F);
    }

    const bool is_surrogate = value >= 0xD800 && value <= 0xDFFF;
    if (value < smallest_for_length || is_surrogate || value > 0x10FFFF) {
        return std::nullopt;
    }
    return DecodedCodePoint{value, byte_length};
}

bool is_forbidden(char32_t code_point) {
    const bool is_c0_control = code_point <= 0x1F;
    const bool is_delete_or_c1_control = code_point >= 0x7F && code_point <= 0x9F;
    const bool is_bidi_embedding_or_override = code_point >= 0x202A && code_point <= 0x202E;
    const bool is_bidi_isolate = code_point >= 0x2066 && code_point <= 0x2069;
    return is_c0_control || is_delete_or_c1_control || is_bidi_embedding_or_override || is_bidi_isolate;
}

bool is_trimmed_whitespace(char32_t code_point) {
    const bool is_en_quad_through_hair_space = code_point >= 0x2000 && code_point <= 0x200A;
    const bool is_line_or_paragraph_separator = code_point == 0x2028 || code_point == 0x2029;
    return code_point == 0x20 || code_point == 0xA0 || code_point == 0x1680 || is_en_quad_through_hair_space ||
           is_line_or_paragraph_separator || code_point == 0x202F || code_point == 0x205F || code_point == 0x3000;
}

}

namespace layer_name {

std::optional<std::string> normalized(std::string_view utf8_text) {
    std::size_t content_begin = std::string_view::npos;
    std::size_t content_end = 0;
    std::size_t code_points_from_begin = 0;
    std::size_t content_code_points = 0;

    for (std::size_t position = 0; position < utf8_text.size();) {
        const std::optional<DecodedCodePoint> decoded = decode_at(utf8_text, position);
        if (!decoded.has_value() || is_forbidden(decoded->value)) {
            return std::nullopt;
        }

        const bool is_whitespace = is_trimmed_whitespace(decoded->value);
        if (content_begin == std::string_view::npos && !is_whitespace) {
            content_begin = position;
        }
        position += decoded->byte_length;
        if (content_begin != std::string_view::npos) {
            ++code_points_from_begin;
            if (!is_whitespace) {
                content_end = position;
                content_code_points = code_points_from_begin;
            }
        }
    }

    if (content_begin == std::string_view::npos) {
        return std::string();
    }
    if (content_code_points > kMaxCodePoints) {
        return std::nullopt;
    }
    return std::string(utf8_text.substr(content_begin, content_end - content_begin));
}

}
