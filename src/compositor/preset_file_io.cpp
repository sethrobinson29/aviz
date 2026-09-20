#include "compositor/preset_file_io.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

namespace preset_file_io {

std::string preset_path(const std::string& filename) {
    return (std::filesystem::path(kPresetsDirectoryName) / filename).string();
}

bool is_safe_preset_filename(const std::string& filename) {
    constexpr std::size_t kMaxLength = 200;
    constexpr std::string_view kExtension = ".json";

    if (filename.empty() || filename.size() > kMaxLength) {
        return false;
    }
    if (filename == "." || filename == "..") {
        return false;
    }
    for (unsigned char c : filename) {
        if (c < 0x20 || c == '/' || c == '\\') {
            return false;
        }
    }
    if (filename.size() <= kExtension.size()) {
        return false;
    }
    return filename.compare(filename.size() - kExtension.size(), kExtension.size(), kExtension) == 0;
}

ReadResult read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return ReadResult{false, {}, "could not open file for reading: " + path};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) {
        return ReadResult{false, {}, "error reading file: " + path};
    }
    return ReadResult{true, buffer.str(), {}};
}

WriteResult write_file(const std::string& path, const std::string& contents) {
    const std::filesystem::path fs_path(path);
    const std::filesystem::path parent = fs_path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return WriteResult{false, "could not create directory " + parent.string() + ": " + ec.message()};
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return WriteResult{false, "could not open file for writing: " + path};
    }
    file << contents;
    if (!file.good()) {
        return WriteResult{false, "error writing file: " + path};
    }
    return WriteResult{true, {}};
}

std::vector<std::string> list_preset_files() {
    std::vector<std::string> names;
    try {
        const std::filesystem::path dir(kPresetsDirectoryName);
        if (!std::filesystem::exists(dir)) {
            return names;
        }
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file()) {
                names.push_back(entry.path().filename().string());
            }
        }
    } catch (const std::filesystem::filesystem_error&) {
        return {};
    }
    std::sort(names.begin(), names.end());
    return names;
}

}
