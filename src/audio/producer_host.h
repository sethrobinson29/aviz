#pragma once

#include <memory>
#include <utility>

#include "audio_producer.h"
#include "ring_buffer.h"

class ProducerHost {
public:
    explicit ProducerHost(AudioRingBuffer& ring, std::unique_ptr<AudioProducer> initial = nullptr)
        : ring_(ring), current_(std::move(initial)) {
        if (current_) {
            current_->start(ring_);
        }
    }

    ~ProducerHost() {
        if (current_) {
            current_->stop();
        }
    }

    ProducerHost(const ProducerHost&) = delete;
    ProducerHost& operator=(const ProducerHost&) = delete;

    void switch_to(std::unique_ptr<AudioProducer> next) {
        if (current_) {
            current_->stop();
        }
        current_ = std::move(next);
        if (current_) {
            current_->start(ring_);
        }
    }

    bool has_producer() const { return current_ != nullptr; }
    AudioProducer* current() const { return current_.get(); }

private:
    AudioRingBuffer& ring_;
    std::unique_ptr<AudioProducer> current_;
};
