#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "audio_config.h"

// Per-frame snapshot handed to every active layer (CLAUDE.md invariant #2).
struct AudioFeatures {
    std::array<float, audio_config::kWaveformSnapshotSize> waveform{};

    // Post-envelope (attack/release smoothed) energy per band.
    std::array<float, audio_config::kTotalBandSlots> band_energy{};
    // Whether a spectral-flux onset triggered this frame, per band.
    std::array<bool, audio_config::kTotalBandSlots> band_onset{};
    // Peak-hold reference per band; always >= audio_config::kAgcMinReference.
    // Visual modules normalize band_energy against this per-band rather
    // than a shared scale.
    std::array<float, audio_config::kTotalBandSlots> band_agc_reference{};

    float rms = 0.0f;
    float peak_frequency_hz = 0.0f;
};

// Triple-buffered AudioFeatures snapshot (CLAUDE.md invariant #1: packed
// index+flag atomic, CAS retry on the consumer side, not a blind exchange).
// Single-writer (worker calls back_buffer()/publish()), single-reader
// (render calls current()).
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
            // compare_exchange_weak refreshed `observed` with the latest value on
            // failure (the producer published concurrently); loop re-checks it.
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
