#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "audio_producer.h"
#include "ring_buffer.h"

class FileProducer : public AudioProducer {
public:
    explicit FileProducer(const std::string& path);
    ~FileProducer() override;

    FileProducer(const FileProducer&) = delete;
    FileProducer& operator=(const FileProducer&) = delete;

    unsigned native_channels() const override;
    unsigned native_sample_rate() const override;

    void start(AudioRingBuffer& ring) override;
    void stop() override;

    bool is_finished() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
