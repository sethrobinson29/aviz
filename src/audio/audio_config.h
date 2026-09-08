#pragma once

#include <cstddef>

namespace audio_config {

inline constexpr unsigned kPipelineSampleRate = 48000;
inline constexpr unsigned kPipelineChannels = 2;

inline constexpr std::size_t kFftWindowSize = 1024;
inline constexpr std::size_t kFftHopSize = 512;

inline constexpr std::size_t kWaveformSnapshotSize = kFftWindowSize;

inline constexpr std::size_t kRingBufferCapacityFrames = 1 << 13;

inline constexpr int kWorkerPollIntervalDivisor = 2;

inline constexpr std::size_t kNumBands = 16;

inline constexpr std::size_t kGlobalBandIndex = kNumBands;
inline constexpr std::size_t kTotalBandSlots = kNumBands + 1;

inline constexpr float kMinBandFrequencyHz = 20.0f;

inline constexpr std::size_t kOnsetHistoryLength = 43;
inline constexpr float kOnsetThresholdMultiplier = 1.5f;
inline constexpr float kOnsetAbsoluteFluxFloor = 0.1f;

inline constexpr float kEnvelopeAttackMs = 10.0f;
inline constexpr float kEnvelopeReleaseMs = 200.0f;

inline constexpr float kIdleFloorFraction = 0.02f;

inline constexpr float kAgcDecayTauMs = 20000.0f;

inline constexpr float kAgcMinReference = 0.01f;

}
