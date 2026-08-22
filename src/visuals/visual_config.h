#pragma once

namespace visual_config {

inline constexpr float kIdleRmsFloor = 0.01f;

// Per-rendered-frame multiplicative decay applied to a BandOnset/GlobalBeat
// pulse (e.g. WaveformVisual::onset_pulse_). Frame-rate-scoped, not
// audio-hop-scoped, since it decays once per render() call.
inline constexpr float kOnsetPulseDecayPerFrame = 0.85f;

// At full pulse (1.0), line/bar thickness is multiplied by this factor.
inline constexpr float kOnsetPulseThicknessBoost = 2.5f;

}  // namespace visual_config
