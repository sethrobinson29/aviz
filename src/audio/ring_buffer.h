#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

// Single-producer/single-consumer lock-free ring buffer for interleaved
// float32 PCM frames. Producer (audio callback) never blocks and never
// allocates; on overflow it overwrites the oldest unread frames rather than
// stalling. Consumer (worker thread) self-corrects if it has fallen behind
// by more than the buffer's capacity, and observes `discontinuity()` to
// know when that happened so it can reset any state that assumes
// continuous history (e.g. per-band onset detection) before trusting the
// next window. See CLAUDE.md invariant #1 and TESTING.md's Concurrency
// section — `write_index_` is written only by the producer, `read_index_`
// only by the consumer.
//
// Slot storage is per-sample std::atomic<float> (relaxed ordering) rather
// than plain float. Under severe overflow (producer laps the consumer more
// than once during a single pop_frames call — only reachable when the
// consumer is already pathologically behind real-time), the producer can
// write into a slot the consumer is concurrently reading. Values read
// during that window can be out of order or repeated — discontinuity()
// says so, and callers must not trust ordering there regardless — but the
// access itself must still be race-free: relaxed atomics guarantee every
// load returns some previously-stored value, never a torn read, without
// imposing any ordering the surrounding index atomics don't already
// provide.
class AudioRingBuffer {
public:
    // `capacity_frames` must be a power of two. `channels` is fixed for the
    // lifetime of the buffer (matches the producer's opened stream).
    AudioRingBuffer(std::size_t capacity_frames, unsigned channels);

    // Producer side.
    void push_frames(const float* interleaved_samples, std::size_t frame_count);

    // Consumer side. Copies up to `frame_count` frames (interleaved,
    // `channels()` wide) into `out`, oldest-unread-first. Returns the
    // number of frames actually copied.
    std::size_t pop_frames(float* out, std::size_t frame_count);

    // Frames currently available to the consumer, clamped to capacity.
    std::size_t available_frames() const;

    unsigned channels() const { return channels_; }
    std::size_t capacity_frames() const { return capacity_frames_; }

    // True if the producer has overwritten frames the consumer had not yet
    // read. Consumer-owned: clear it once dependent state has been reset.
    bool discontinuity() const;
    void clear_discontinuity();

private:
    std::vector<std::atomic<float>> buffer_;
    std::size_t capacity_frames_;
    std::size_t capacity_mask_;
    unsigned channels_;

    std::atomic<std::size_t> write_index_{0};
    std::atomic<std::size_t> read_index_{0};
    std::atomic<bool> discontinuity_{false};
};
