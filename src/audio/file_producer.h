#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "ring_buffer.h"

// Local file playback producer (spec §3, milestone 1). Decodes and plays a
// file through the real audio device via miniaudio while pushing the same
// frames into the shared AudioRingBuffer through push_frames() — the same
// interface loopback capture will use later (CLAUDE.md invariant #5).
//
// Pimpl'd so this header never includes miniaudio.h: miniaudio.h pulls in
// <windows.h>, which collides with raylib.h's symbols if both land in one
// translation unit (see miniaudio_backend.h/.cpp and commit 50f257f). Any
// caller including this header alongside raylib.h stays safe.
class FileProducer {
public:
    // Opens and decodes the file's header (does not start playback).
    // Throws std::runtime_error with a plain description if the file can't
    // be opened or decoded — no silent fallback (CLAUDE.md working style).
    explicit FileProducer(const std::string& path);
    ~FileProducer();

    FileProducer(const FileProducer&) = delete;
    FileProducer& operator=(const FileProducer&) = delete;

    unsigned channels() const;
    unsigned sample_rate() const;

    // Opens the playback device and starts audio flowing into `ring`.
    // `ring` must outlive the producer (or until stop() is called) and must
    // have been constructed with channels() channels. Throws
    // std::runtime_error if the device can't be opened (e.g. no audio
    // hardware — TESTING.md's no-hardware fallback applies to callers, not
    // hidden here).
    void start(AudioRingBuffer& ring);
    void stop();

    // True once the decoder has reached end-of-file (device continues
    // outputting silence until stop() is called).
    bool is_finished() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
