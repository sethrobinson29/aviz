#pragma once

#include <cstddef>

// Tunable constants for the audio analysis pipeline. Kept centralized and
// named (not inline literals) so they're easy to find and retune later —
// see visualizer-design-spec.md §8 milestone 1 design discussion.
namespace audio_config {

// FFT analysis window, in samples. 1024/512 (50% overlap) balances
// frequency resolution against onset-timing responsiveness.
inline constexpr std::size_t kFftWindowSize = 1024;

// Hop size between successive analysis windows, in samples.
inline constexpr std::size_t kFftHopSize = 512;

// Raw waveform snapshot size handed to AudioFeatures consumers, in samples.
// Shares the FFT window's time-domain buffer rather than maintaining a
// second one.
inline constexpr std::size_t kWaveformSnapshotSize = kFftWindowSize;

// SPSC ring buffer capacity, in frames per channel. Sized to absorb worker
// thread scheduling jitter (~16x kFftHopSize), not steady-state buffering —
// the worker drains near-empty each poll under normal load. Kept tight
// deliberately: a larger capacity would mask a real worker stall for
// longer before discontinuity()/overwrite-on-full ever kicks in. ~170-186ms
// of audio at typical 44.1-48kHz rates.
inline constexpr std::size_t kRingBufferCapacityFrames = 1 << 13;

// The worker polls at (kFftHopSize / sample_rate) / kWorkerPollIntervalDivisor
// seconds, computed at runtime once the producer's actual sample rate is
// known. This bounds added latency to a fraction of one hop's worth of
// audio without any signal/condvar touching the audio callback.
inline constexpr int kWorkerPollIntervalDivisor = 2;

// Log-scaled frequency bands (20Hz-Nyquist), per CLAUDE.md invariant #2
// (visual modules consume banded energies, not raw FFT bins).
inline constexpr std::size_t kNumBands = 16;

// Global/full-spectrum band per CLAUDE.md invariant #3 ("global beat is
// band index kGlobalBandIndex, not a separate detector") — one extra slot
// appended after the real bands, computed with the same per-band
// spectral-flux onset logic scoped to full-spectrum energy.
inline constexpr std::size_t kGlobalBandIndex = kNumBands;
inline constexpr std::size_t kTotalBandSlots = kNumBands + 1;

}  // namespace audio_config
