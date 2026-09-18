#pragma once

#include <string>
#include <vector>

namespace preset_file_io {

inline constexpr const char* kPresetsDirectoryName = "presets";

std::string preset_path(const std::string& filename);

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
std::vector<std::string> list_preset_files();

}
