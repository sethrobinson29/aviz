#include "audio/audio_file_validation.h"

#include <array>
#include <cctype>
#include <cstddef>

namespace audio_file_validation {

namespace {

constexpr std::array<const char*, 3> kSupportedExtensions{".wav", ".mp3", ".flac"};

bool has_control_character(const std::string& path) {
    for (unsigned char c : path) {
        if (c < 0x20) {
            return true;
        }
    }
    return false;
}

bool ends_with_supported_extension(const std::string& path) {
    for (const char* extension : kSupportedExtensions) {
        std::size_t len = std::string(extension).size();
        if (path.size() < len) {
            continue;
        }
        bool matches = true;
        for (std::size_t i = 0; i < len; ++i) {
            if (std::tolower(static_cast<unsigned char>(path[path.size() - len + i])) != extension[i]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

}

bool is_plausible_audio_file_path(const std::string& path) {
    if (path.empty() || has_control_character(path)) {
        return false;
    }
    return ends_with_supported_extension(path);
}

}
