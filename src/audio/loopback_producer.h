#pragma once

#include <memory>

#include "audio_producer.h"
#include "ring_buffer.h"

// WASAPI loopback capture -- taps whatever is currently playing on the
// default output device. Windows-only (see CLAUDE.md's Target Platform
// section); loopback is only supported by miniaudio's WASAPI backend.
class LoopbackProducer : public AudioProducer {
public:
    // Throws std::runtime_error if the loopback device can't be opened.
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
