#pragma once

#include <cstddef>

namespace audio_config {

inline constexpr std::size_t kFftWindowSize = 1024;
inline constexpr std::size_t kFftHopSize = 512;

inline constexpr std::size_t kWaveformSnapshotSize = kFftWindowSize;

inline constexpr std::size_t kRingBufferCapacityFrames = 1 << 13;

// Worker poll interval = (kFftHopSize / sample_rate) / this.
inline constexpr int kWorkerPollIntervalDivisor = 2;

// Log-scaled frequency bands, 20 Hz to Nyquist.
inline constexpr std::size_t kNumBands = 16;

inline constexpr std::size_t kGlobalBandIndex = kNumBands;
inline constexpr std::size_t kTotalBandSlots = kNumBands + 1;

// High edge is always the runtime Nyquist frequency (sample_rate / 2).
inline constexpr float kMinBandFrequencyHz = 20.0f;

// Onset fires when spectral flux exceeds mean + kOnsetThresholdMultiplier *
// stddev + kOnsetFluxEpsilon, over the trailing kOnsetHistoryLength flux
// values for that band.
inline constexpr std::size_t kOnsetHistoryLength = 43;
inline constexpr float kOnsetThresholdMultiplier = 1.5f;
inline constexpr float kOnsetFluxEpsilon = 0.01f;

inline constexpr float kEnvelopeAttackMs = 10.0f;
inline constexpr float kEnvelopeReleaseMs = 200.0f;

// Rise is an exact snap to any new peak (no tau); this constant covers
// decay only.
inline constexpr float kAgcDecayTauMs = 20000.0f;

// Also the seed value a band's AGC reference starts at on the first
// process() call.
inline constexpr float kAgcMinReference = 0.01f;

}  // namespace audio_config
