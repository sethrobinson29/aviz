#pragma once

#include <memory>
#include <utility>

#include "audio_producer.h"
#include "ring_buffer.h"

class ProducerHost {
public:
    ProducerHost(AudioRingBuffer& ring, std::unique_ptr<AudioProducer> initial)
        : ring_(ring), current_(std::move(initial)) {
        current_->start(ring_);
    }

    ~ProducerHost() { current_->stop(); }

    ProducerHost(const ProducerHost&) = delete;
    ProducerHost& operator=(const ProducerHost&) = delete;

    void switch_to(std::unique_ptr<AudioProducer> next) {
        current_->stop();
        current_ = std::move(next);
        current_->start(ring_);
    }

    AudioProducer& current() const { return *current_; }

private:
    AudioRingBuffer& ring_;
    std::unique_ptr<AudioProducer> current_;
};
