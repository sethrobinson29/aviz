#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "ring_buffer.h"

// Local file playback producer (CLAUDE.md invariant #5).
//
// Pimpl'd so this header never includes miniaudio.h: miniaudio.h pulls in
// <windows.h>, which collides with raylib.h's symbols in the same
// translation unit.
class FileProducer {
public:
    // Throws std::runtime_error if the file can't be opened or decoded.
    explicit FileProducer(const std::string& path);
    ~FileProducer();

    FileProducer(const FileProducer&) = delete;
    FileProducer& operator=(const FileProducer&) = delete;

    unsigned channels() const;
    unsigned sample_rate() const;

    // `ring` must outlive the producer and be constructed with channels()
    // channels. Throws std::runtime_error if the device can't be opened.
    void start(AudioRingBuffer& ring);
    void stop();

    // True at end-of-file; device continues outputting silence until stop().
    bool is_finished() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
