#pragma once

#include <cstddef>

// Tunable constants for the audio analysis pipeline.
namespace audio_config {

// FFT analysis window, in samples. 1024/512 (50% overlap).
inline constexpr std::size_t kFftWindowSize = 1024;
inline constexpr std::size_t kFftHopSize = 512;

// Shares the FFT window's time-domain buffer rather than maintaining a
// second one.
inline constexpr std::size_t kWaveformSnapshotSize = kFftWindowSize;

// SPSC ring buffer capacity, in frames per channel. Sized to absorb worker
// scheduling jitter (~16x kFftHopSize), not steady-state buffering -- kept
// tight so a real worker stall triggers discontinuity()/overwrite sooner.
inline constexpr std::size_t kRingBufferCapacityFrames = 1 << 13;

// Worker poll interval = (kFftHopSize / sample_rate) / this.
inline constexpr int kWorkerPollIntervalDivisor = 2;

// Log-scaled frequency bands (20Hz-Nyquist), per CLAUDE.md invariant #2.
inline constexpr std::size_t kNumBands = 16;

// Global/full-spectrum band per CLAUDE.md invariant #3 -- one extra slot
// after the real bands, not prepended/interleaved.
inline constexpr std::size_t kGlobalBandIndex = kNumBands;
inline constexpr std::size_t kTotalBandSlots = kNumBands + 1;

// High edge is always the runtime Nyquist frequency (sample_rate / 2).
inline constexpr float kMinBandFrequencyHz = 20.0f;

// Onset fires when spectral flux exceeds mean + kOnsetThresholdMultiplier *
// stddev over the trailing kOnsetHistoryLength flux values for that band.
inline constexpr std::size_t kOnsetHistoryLength = 43;
inline constexpr float kOnsetThresholdMultiplier = 1.5f;

// Absolute floor added to the adaptive onset threshold -- without it, a
// band with near-zero flux history has stddev ~0, so any nonzero flux
// fires regardless of size.
inline constexpr float kOnsetFluxEpsilon = 0.01f;

// One-pole attack/release envelope applied to band energy before
// publishing (AudioFeatures::band_energy is post-envelope).
inline constexpr float kEnvelopeAttackMs = 10.0f;
inline constexpr float kEnvelopeReleaseMs = 200.0f;

// Per-band AGC reference used to normalize that band for display
// independent of the other bands' scale. Rise is an exact snap to any new
// peak (no tau); this is the decay-side time constant only.
inline constexpr float kAgcDecayTauMs = 20000.0f;

// Floor under the AGC reference -- prevents divide-by-near-zero, and is
// the minimum a band's reference seeds to on the first process() call.
inline constexpr float kAgcMinReference = 0.01f;

}  // namespace audio_config
