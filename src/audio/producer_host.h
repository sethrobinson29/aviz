#pragma once

#include <memory>
#include <utility>

#include "audio_producer.h"
#include "ring_buffer.h"

// Owns the single active AudioProducer and is the only code path that may
// change which one is active. switch_to() is the only mutator: it always
// stops the current producer to completion (see AudioProducer::stop()'s
// postcondition) before starting the next one, so that ordering can't be
// violated by a caller reaching start()/stop() directly -- there is no
// other public path to either.
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
