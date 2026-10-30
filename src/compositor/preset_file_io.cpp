#include "compositor/preset_file_io.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

namespace {

constexpr std::string_view kCharactersInvalidInWindowsFilenames = "<>:\"|?*";

constexpr std::array<std::string_view, 22> kWindowsReservedDeviceNames{
    "CON",  "PRN",  "AUX",  "NUL",  "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
    "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

bool is_windows_reserved_device_name(std::string_view filename) {
    std::string stem(filename.substr(0, filename.find('.')));
    while (!stem.empty() && stem.back() == ' ') {
        stem.pop_back();
    }
    for (char& c : stem) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return std::find(kWindowsReservedDeviceNames.begin(), kWindowsReservedDeviceNames.end(), stem) !=
           kWindowsReservedDeviceNames.end();
}

}

namespace preset_file_io {

std::string presets_directory_in(const std::string& application_dir) {
    return std::filesystem::absolute(std::filesystem::path(application_dir) / kPresetsDirectoryName)
        .lexically_normal()
        .string();
}

std::string preset_path(const std::string& presets_dir, const std::string& filename) {
    return (std::filesystem::path(presets_dir) / filename).string();
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
        if (c < 0x20 || c == 0x7F || c == '/' || c == '\\') {
            return false;
        }
        if (kCharactersInvalidInWindowsFilenames.find(static_cast<char>(c)) != std::string_view::npos) {
            return false;
        }
    }
    if (is_windows_reserved_device_name(filename)) {
        return false;
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

std::vector<std::string> list_preset_files(const std::string& presets_dir) {
    std::vector<std::string> names;
    try {
        const std::filesystem::path dir(presets_dir);
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
