#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "audio_config.h"

// Per-frame snapshot handed to every active layer. Visual modules consume
// only this struct, filtered to their assigned band if applicable — never
// raw PCM or FFT bins directly (CLAUDE.md invariant #2). Band index
// audio_config::kGlobalBandIndex is the full-spectrum/global band, computed
// with the same per-band spectral-flux onset logic as every other band
// (CLAUDE.md invariant #3), not a separate detector.
struct AudioFeatures {
    std::array<float, audio_config::kWaveformSnapshotSize> waveform{};

    // Post-envelope (attack/release smoothed) energy per band.
    std::array<float, audio_config::kTotalBandSlots> band_energy{};
    // Whether a spectral-flux onset triggered this frame, per band.
    std::array<bool, audio_config::kTotalBandSlots> band_onset{};

    float rms = 0.0f;
    float peak_frequency_hz = 0.0f;
};

// Triple-buffered AudioFeatures snapshot (CLAUDE.md invariant #1). A plain
// 2-buffer ping-pong swapped via a single atomic index is NOT race-free
// here: current() returns a reference render reads across several
// statements, and nothing stops the worker from publishing repeatedly and
// wrapping back into that exact buffer while render is still mid-read of
// it — a genuine torn-read data race, caught by
// AudioFeaturesSnapshotConcurrencyTest before this was switched to triple
// buffering. (TESTING.md's Concurrency section says "double-buffering with
// an atomic index/pointer"; that wording under-specifies what's actually
// needed for an unsynchronized reader/writer pair and should be flagged
// stale there.)
//
// Triple buffering fixes this structurally: the worker exclusively owns a
// private back buffer, the render thread exclusively owns a private front
// buffer, and a third "middle" buffer changes hands between them via a
// shared atomic word that packs the middle buffer's index together with a
// "new data" flag in one value (`shared_`). Packing them together matters:
// an earlier version of this class stored the index and the flag as two
// separate atomics, which left a window where the flag could be observed
// out of sync with the index it was meant to describe — not a torn read,
// but a genuine regression bug (render could receive an older buffer than
// one it already had). The producer side (publish()) can unconditionally
// exchange(), since it always wants to hand over its fresh buffer
// regardless of prior state. The consumer side (current()) must only swap
// when the flag is confirmed set, so it uses a compare_exchange retry loop
// — a blind exchange there would occasionally swap in the same stale
// buffer it just gave up. Neither side ever blocks; the consumer's CAS
// loop is bounded by how many times the producer can publish concurrently,
// not an indefinite spin.
//
// Single-writer (worker calls back_buffer()/publish()), single-reader
// (render calls current()). Three preallocated instances only — no
// per-frame allocation.
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
