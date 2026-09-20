#pragma once

#include <string>

#include "audio/producer_host.h"

class AudioSourceController {
public:
    struct SwitchResult {
        bool success = false;
        std::string error;
        unsigned channels = 0;
        unsigned sample_rate = 0;
    };

    AudioSourceController(ProducerHost& producer_host, std::string file_path);

    bool is_loopback() const { return using_loopback_; }

    SwitchResult toggle();

private:
    ProducerHost& producer_host_;
    std::string file_path_;
    bool using_loopback_ = false;
};
