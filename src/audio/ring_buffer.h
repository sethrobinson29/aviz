#pragma once

#include <atomic>
#include <cstddef>
#include <vector>

class AudioRingBuffer {
public:
    AudioRingBuffer(std::size_t capacity_frames, unsigned channels);

    void push_frames(const float* interleaved_samples, std::size_t frame_count);

    std::size_t pop_frames(float* out, std::size_t frame_count);

    std::size_t available_frames() const;

    unsigned channels() const { return channels_; }
    std::size_t capacity_frames() const { return capacity_frames_; }

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
