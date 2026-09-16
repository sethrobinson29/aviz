#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "audio_config.h"

struct AudioFeatures {
    std::array<float, audio_config::kWaveformSnapshotSize> waveform{};
    std::array<float, audio_config::kWaveformSnapshotSize> waveform_l{};
    std::array<float, audio_config::kWaveformSnapshotSize> waveform_r{};

    std::array<float, audio_config::kTotalBandSlots> band_energy{};
    std::array<bool, audio_config::kTotalBandSlots> band_idle{};
    std::array<bool, audio_config::kTotalBandSlots> band_onset{};
    std::array<float, audio_config::kTotalBandSlots> band_agc_reference{};

    float rms = 0.0f;
    float peak_frequency_hz = 0.0f;
};

class AudioFeaturesSnapshot {
public:
    AudioFeatures& back_buffer() { return buffers_[back_index_]; }

    void publish() {
        unsigned new_shared = static_cast<unsigned>(back_index_) | kNewDataFlag;
        unsigned old_shared = shared_.exchange(new_shared, std::memory_order_acq_rel);
        back_index_ = old_shared & kIndexMask;
    }

    const AudioFeatures& current() {
        unsigned observed = shared_.load(std::memory_order_acquire);
        while (observed & kNewDataFlag) {
            if (shared_.compare_exchange_weak(observed, static_cast<unsigned>(front_index_),
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
                front_index_ = observed & kIndexMask;
                break;
            }
        }
        return buffers_[front_index_];
    }

private:
    static constexpr unsigned kIndexMask = 0x3;
    static constexpr unsigned kNewDataFlag = 0x4;

    AudioFeatures buffers_[3]{};
    std::size_t back_index_ = 0;
    std::atomic<unsigned> shared_{1};
    std::size_t front_index_ = 2;
};
