#pragma once

class AudioRingBuffer;

class AudioProducer {
public:
    virtual ~AudioProducer() = default;

    virtual void start(AudioRingBuffer& ring) = 0;

    virtual void stop() = 0;

    virtual unsigned native_channels() const = 0;
    virtual unsigned native_sample_rate() const = 0;
};
