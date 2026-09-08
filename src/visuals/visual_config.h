#pragma once

namespace visual_config {

inline constexpr float kOnsetPulseDecayPerFrame = 0.85f;

inline constexpr float kOnsetPulseThicknessBoost = 2.5f;

inline constexpr float kSpectrumOnsetPulseDecayPerFrame = 0.92f;

inline constexpr float kSpectrumOnsetPulseRestBrightnessFraction = 0.55f;

inline constexpr float kMinBarHeightPx = 4.0f;

inline constexpr int kMaxParticles = 1024;

inline constexpr float kParticleBaseSpeedPxPerFrame = 4.0f;

inline constexpr float kParticleAngleJitterRadians = 0.35f;

inline constexpr int kParticleDefaultLifetimeFrames = 150;

}
