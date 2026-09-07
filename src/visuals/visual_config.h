#pragma once

namespace visual_config {

// Per-rendered-frame multiplicative decay applied to a BandOnset/GlobalBeat
// pulse (e.g. WaveformVisual::onset_pulse_). Frame-rate-scoped, not
// audio-hop-scoped, since it decays once per render() call.
inline constexpr float kOnsetPulseDecayPerFrame = 0.85f;

// At full pulse (1.0), line/bar thickness is multiplied by this factor.
inline constexpr float kOnsetPulseThicknessBoost = 2.5f;

// Per-rendered-frame multiplicative decay applied to SpectrumVisual's
// per-band onset color pulses.
inline constexpr float kSpectrumOnsetPulseDecayPerFrame = 0.92f;

// Fraction of bar_color's own channel values an onset/GlobalBeat-scoped bar
// rests at with no pulse in flight; rises to full bar_color (unchanged) at
// pulse == 1. Tunable; needs tuning against real material.
inline constexpr float kSpectrumOnsetPulseRestBrightnessFraction = 0.55f;

// Floor on a rendered spectrum bar's pixel height -- a bar at rest (or
// genuinely idle) renders at this height rather than disappearing to 0px.
// Initial guess, unconfirmed against real material; tunable.
inline constexpr float kMinBarHeightPx = 4.0f;

}  // namespace visual_config
