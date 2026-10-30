#pragma once

#include <string>
#include <vector>

namespace preset_file_io {

inline constexpr const char* kPresetsDirectoryName = "presets";

std::string presets_directory_in(const std::string& application_dir);
std::string preset_path(const std::string& presets_dir, const std::string& filename);
bool is_safe_preset_filename(const std::string& filename);

struct ReadResult {
    bool success = false;
    std::string contents;
    std::string error;
};

struct WriteResult {
    bool success = false;
    std::string error;
};

ReadResult read_file(const std::string& path);
WriteResult write_file(const std::string& path, const std::string& contents);
std::vector<std::string> list_preset_files(const std::string& presets_dir);

}
