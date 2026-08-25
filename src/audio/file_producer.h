#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "audio_producer.h"
#include "ring_buffer.h"

class FileProducer : public AudioProducer {
public:
    // Throws std::runtime_error if the file can't be opened or decoded.
    explicit FileProducer(const std::string& path);
    ~FileProducer() override;

    FileProducer(const FileProducer&) = delete;
    FileProducer& operator=(const FileProducer&) = delete;

    unsigned native_channels() const override;
    unsigned native_sample_rate() const override;

    // Throws std::runtime_error if the device can't be opened.
    void start(AudioRingBuffer& ring) override;
    void stop() override;

    // True at end-of-file; device continues outputting silence until stop().
    bool is_finished() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
