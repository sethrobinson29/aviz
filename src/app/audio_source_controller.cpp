#include "app/audio_source_controller.h"

#include <memory>

#include "audio/file_producer.h"
#include "audio/loopback_producer.h"

AudioSourceController::AudioSourceController(ProducerHost& producer_host, std::string file_path)
    : producer_host_(producer_host), file_path_(std::move(file_path)) {}

AudioSourceController::SwitchResult AudioSourceController::toggle() {
    try {
        if (using_loopback_) {
            producer_host_.switch_to(std::make_unique<FileProducer>(file_path_));
            using_loopback_ = false;
        } else {
            producer_host_.switch_to(std::make_unique<LoopbackProducer>());
            using_loopback_ = true;
        }
        return SwitchResult{true, {}, producer_host_.current().native_channels(),
                             producer_host_.current().native_sample_rate()};
    } catch (const std::exception& e) {
        return SwitchResult{false, e.what(), 0, 0};
    }
}
