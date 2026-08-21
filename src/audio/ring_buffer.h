#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

// Single-producer/single-consumer lock-free ring buffer for interleaved
// float32 PCM frames. Producer never blocks; on overflow it overwrites the
// oldest unread frames and sets discontinuity().
class AudioRingBuffer {
public:
    // `capacity_frames` must be a power of two. `channels` is fixed for the
    // lifetime of the buffer (matches the producer's opened stream).
    AudioRingBuffer(std::size_t capacity_frames, unsigned channels);

    void push_frames(const float* interleaved_samples, std::size_t frame_count);

    // Copies up to `frame_count` frames (interleaved, `channels()` wide)
    // into `out`, oldest-unread-first. Returns the number of frames
    // actually copied.
    std::size_t pop_frames(float* out, std::size_t frame_count);

    // Frames currently available to the consumer, clamped to capacity.
    std::size_t available_frames() const;

    unsigned channels() const { return channels_; }
    std::size_t capacity_frames() const { return capacity_frames_; }

    bool discontinuity() const;
    // Call once dependent state has been reset.
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
