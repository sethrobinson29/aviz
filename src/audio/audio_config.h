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

// Low edge of the log-scaled band ladder. High edge is always the runtime
// Nyquist frequency (sample_rate / 2), not a fixed constant, since it varies
// by producer.
inline constexpr float kMinBandFrequencyHz = 20.0f;

// Per-band onset detection: fire when spectral flux exceeds
// mean + kOnsetThresholdMultiplier * stddev over the trailing
// kOnsetHistoryLength flux values for that band (spec §4). History length
// is ~460ms at the nominal ~93.75Hz update rate (hop 512 @ 48kHz) — long
// enough to characterize a band's noise floor, short enough to adapt to a
// section change (verse -> chorus) within a beat or two.
inline constexpr std::size_t kOnsetHistoryLength = 43;
inline constexpr float kOnsetThresholdMultiplier = 1.5f;

// Absolute floor added to the adaptive threshold. Without it, a band whose
// recent flux history is all/near-zero (silence, or a very stable quiet
// background) has stddev ~0, so the threshold collapses to ~0 and any
// nonzero flux fires regardless of size — including FFT windowing leakage
// from a transient in a completely different band. This is a blunt,
// per-band-energy-scale-agnostic floor (band energy magnitude varies ~4
// orders of magnitude bass-to-presence), so it's a starting point for
// by-ear/by-eye tuning, not a precisely derived value.
inline constexpr float kOnsetFluxEpsilon = 0.01f;

// One-pole exponential envelope applied to band energy before it's
// published (AudioFeatures::band_energy is post-envelope, spec §4).
// Fast attack so transients still read as sharp, slower release so bars
// don't visually chatter between updates.
inline constexpr float kEnvelopeAttackMs = 10.0f;
inline constexpr float kEnvelopeReleaseMs = 200.0f;

}  // namespace audio_config
