#pragma once

#include <cstddef>
#include <string>

namespace preset_file_io {

inline constexpr const char* kPresetsDirectoryName = "presets";
inline constexpr std::size_t kMaxPresetFileBytes = 1024 * 1024;

std::string presets_directory_in(const std::string& application_dir);
std::string preset_path(const std::string& presets_dir, const std::string& filename);
bool is_safe_preset_filename(const std::string& filename);

struct ReadResult {
    bool success = false;
    std::string contents;
    std::string error;
    bool too_large = false;
};

struct WriteResult {
    bool success = false;
    std::string error;
};

ReadResult read_file(const std::string& path, std::size_t max_bytes = kMaxPresetFileBytes);
WriteResult write_file(const std::string& path, const std::string& contents);

}
