#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "audio_config.h"

struct AudioFeatures {
    std::array<float, audio_config::kWaveformSnapshotSize> waveform{};

    // Post-envelope (attack/release smoothed) energy per band. Keeps
    // decaying under its own envelope math even once band_idle is true --
    // never force-set to exactly 0.
    std::array<float, audio_config::kTotalBandSlots> band_energy{};
    // True once band_energy has decayed below audio_config::kIdleFloorFraction
    // of the band's own AGC reference.
    std::array<bool, audio_config::kTotalBandSlots> band_idle{};
    std::array<bool, audio_config::kTotalBandSlots> band_onset{};
    // Peak-hold reference per band; always >= audio_config::kAgcMinReference.
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
    std::size_t back_index_ = 0;               // worker-owned only
    std::atomic<unsigned> shared_{1};          // packed: middle buffer index + new-data flag
    std::size_t front_index_ = 2;              // render-owned only
};
