#include "ring_buffer.h"

#include <cassert>

namespace {
bool is_power_of_two(std::size_t value) {
    return value != 0 && (value & (value - 1)) == 0;
}
}  // namespace

AudioRingBuffer::AudioRingBuffer(std::size_t capacity_frames, unsigned channels)
    : buffer_(capacity_frames * channels),
      capacity_frames_(capacity_frames),
      capacity_mask_(capacity_frames - 1),
      channels_(channels) {
    assert(is_power_of_two(capacity_frames) && "capacity_frames must be a power of two");
    assert(channels > 0);
}

void AudioRingBuffer::push_frames(const float* interleaved_samples, std::size_t frame_count) {
    std::size_t write = write_index_.load(std::memory_order_relaxed);

    for (std::size_t i = 0; i < frame_count; ++i) {
        std::size_t slot = (write + i) & capacity_mask_;
        for (unsigned c = 0; c < channels_; ++c) {
            buffer_[slot * channels_ + c].store(interleaved_samples[i * channels_ + c],
                                                 std::memory_order_relaxed);
        }
    }

    std::size_t new_write = write + frame_count;

    std::size_t read = read_index_.load(std::memory_order_acquire);
    if (new_write - read > capacity_frames_) {
        discontinuity_.store(true, std::memory_order_relaxed);
    }

    write_index_.store(new_write, std::memory_order_release);
}

std::size_t AudioRingBuffer::pop_frames(float* out, std::size_t frame_count) {
    std::size_t write = write_index_.load(std::memory_order_acquire);
    std::size_t read = read_index_.load(std::memory_order_relaxed);

    // Producer has lapped us: snap forward to the oldest still-valid frame.
    if (write - read > capacity_frames_) {
        read = write - capacity_frames_;
    }

    std::size_t avail = write - read;
    std::size_t to_copy = frame_count < avail ? frame_count : avail;

    for (std::size_t i = 0; i < to_copy; ++i) {
        std::size_t slot = (read + i) & capacity_mask_;
        for (unsigned c = 0; c < channels_; ++c) {
            out[i * channels_ + c] = buffer_[slot * channels_ + c].load(std::memory_order_relaxed);
        }
    }

    read_index_.store(read + to_copy, std::memory_order_release);
    return to_copy;
}

std::size_t AudioRingBuffer::available_frames() const {
    std::size_t write = write_index_.load(std::memory_order_acquire);
    std::size_t read = read_index_.load(std::memory_order_relaxed);
    std::size_t avail = write - read;
    return avail > capacity_frames_ ? capacity_frames_ : avail;
}

bool AudioRingBuffer::discontinuity() const {
    return discontinuity_.load(std::memory_order_relaxed);
}

void AudioRingBuffer::clear_discontinuity() {
    discontinuity_.store(false, std::memory_order_relaxed);
}
