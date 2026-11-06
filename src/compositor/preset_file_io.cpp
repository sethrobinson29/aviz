#include "compositor/preset_file_io.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>

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
        .u8string();
}

std::string preset_path(const std::string& presets_dir, const std::string& filename) {
    return (std::filesystem::u8path(presets_dir) / std::filesystem::u8path(filename)).u8string();
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

ReadResult read_file(const std::string& path, std::size_t max_bytes) {
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file.is_open()) {
        return ReadResult{false, {}, "could not open file for reading: " + path};
    }
    std::string contents(max_bytes + 1, char{});
    file.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (file.bad()) {
        return ReadResult{false, {}, "error reading file: " + path};
    }
    const std::size_t bytes_read = static_cast<std::size_t>(file.gcount());
    if (bytes_read > max_bytes) {
        return ReadResult{false, {}, "file is larger than " + std::to_string(max_bytes) + " bytes: " + path, true};
    }
    contents.resize(bytes_read);
    return ReadResult{true, std::move(contents), {}};
}

WriteResult write_file(const std::string& path, const std::string& contents) {
    const std::filesystem::path fs_path = std::filesystem::u8path(path);
    const std::filesystem::path parent = fs_path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return WriteResult{false, "could not create directory " + parent.u8string() + ": " + ec.message()};
        }
    }

    std::ofstream file(fs_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return WriteResult{false, "could not open file for writing: " + path};
    }
    file << contents;
    if (!file.good()) {
        return WriteResult{false, "error writing file: " + path};
    }
    return WriteResult{true, {}};
}

}
