#pragma once

#include <string>

#include "audio/producer_host.h"

enum class AudioSourceKind { None, File, Loopback };

class AudioSourceController {
public:
    struct SwitchResult {
        bool success = false;
        std::string error;
        unsigned channels = 0;
        unsigned sample_rate = 0;
    };

    explicit AudioSourceController(ProducerHost& producer_host);

    AudioSourceKind kind() const { return kind_; }
    bool has_source() const { return kind_ != AudioSourceKind::None; }
    bool is_loopback() const { return kind_ == AudioSourceKind::Loopback; }
    const std::string& current_file_path() const { return file_path_; }

    SwitchResult import_file(std::string path);
    SwitchResult enable_loopback();
    SwitchResult toggle();

private:
    SwitchResult switch_to_file(const std::string& path);
    SwitchResult switch_to_loopback();

    ProducerHost& producer_host_;
    AudioSourceKind kind_ = AudioSourceKind::None;
    std::string file_path_;
};
