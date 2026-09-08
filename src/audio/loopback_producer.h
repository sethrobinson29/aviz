#pragma once

#include <memory>

#include "audio_producer.h"
#include "ring_buffer.h"

class LoopbackProducer : public AudioProducer {
public:
    LoopbackProducer();
    ~LoopbackProducer() override;

    LoopbackProducer(const LoopbackProducer&) = delete;
    LoopbackProducer& operator=(const LoopbackProducer&) = delete;

    unsigned native_channels() const override;
    unsigned native_sample_rate() const override;

    void start(AudioRingBuffer& ring) override;
    void stop() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
