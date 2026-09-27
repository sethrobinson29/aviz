#include "app/audio_source_controller.h"

#include <memory>

#include "audio/audio_file_validation.h"
#include "audio/file_producer.h"
#include "audio/loopback_producer.h"

AudioSourceController::AudioSourceController(ProducerHost& producer_host) : producer_host_(producer_host) {}

AudioSourceController::SwitchResult AudioSourceController::switch_to_file(const std::string& path) {
    try {
        producer_host_.switch_to(std::make_unique<FileProducer>(path));
        kind_ = AudioSourceKind::File;
        file_path_ = path;
        return SwitchResult{true, {}, producer_host_.current()->native_channels(),
                             producer_host_.current()->native_sample_rate()};
    } catch (const std::exception& e) {
        return SwitchResult{false, e.what(), 0, 0};
    }
}

AudioSourceController::SwitchResult AudioSourceController::switch_to_loopback() {
    try {
        producer_host_.switch_to(std::make_unique<LoopbackProducer>());
        kind_ = AudioSourceKind::Loopback;
        return SwitchResult{true, {}, producer_host_.current()->native_channels(),
                             producer_host_.current()->native_sample_rate()};
    } catch (const std::exception& e) {
        return SwitchResult{false, e.what(), 0, 0};
    }
}

AudioSourceController::SwitchResult AudioSourceController::import_file(std::string path) {
    if (!audio_file_validation::is_plausible_audio_file_path(path)) {
        return SwitchResult{false, "not a supported audio file path: " + path, 0, 0};
    }
    return switch_to_file(path);
}

AudioSourceController::SwitchResult AudioSourceController::enable_loopback() { return switch_to_loopback(); }

AudioSourceController::SwitchResult AudioSourceController::toggle() {
    if (kind_ == AudioSourceKind::None) {
        return SwitchResult{false, "no audio source has been selected yet", 0, 0};
    }
    if (kind_ == AudioSourceKind::Loopback) {
        if (file_path_.empty()) {
            return SwitchResult{false, "no file has been imported to switch back to", 0, 0};
        }
        return switch_to_file(file_path_);
    }
    return switch_to_loopback();
}
