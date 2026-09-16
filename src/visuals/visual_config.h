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

inline constexpr int kFlowFieldGridCols = 32;

inline constexpr int kFlowFieldGridRows = 18;

inline constexpr int kFlowFieldMaxGridCells = kFlowFieldGridCols * kFlowFieldGridRows;

inline constexpr float kFlowFieldBaseTurbulenceFrequency = 0.01f;

inline constexpr float kFlowFieldBaseEvolutionPerFrame = 0.01f;

inline constexpr float kFlowFieldSteerFactor = 0.15f;

inline constexpr float kFlowFieldSpeedPxPerFrame = 2.0f;

inline constexpr int kFlowFieldDefaultLifetimeFrames = 400;

inline constexpr float kFlowFieldDefaultSpawnRatePerFrame = 0.75f;

inline constexpr int kFlowFieldDefaultBurstCount = 16;

inline constexpr int kMaxFlowFieldRipples = 4;

inline constexpr int kFlowFieldRippleLifetimeFrames = 40;

inline constexpr float kFlowFieldRippleSpeedPxPerFrame = 8.0f;

inline constexpr float kFlowFieldRippleBandwidthPx = 40.0f;

inline constexpr float kFlowFieldRippleStrength = 3.0f;

inline constexpr int kFractalMaxIterations = 200;

inline constexpr float kFractalBaseZoom = 1.0f;

inline constexpr float kFractalZoomAmplitudeScale = 0.5f;

inline constexpr float kFractalJuliaBaseRadius = 0.7885f;

inline constexpr float kFractalJuliaRadiusAmplitudeScale = 0.05f;

inline constexpr float kFractalJuliaAngularSpeedRadiansPerSec = 0.15f;

inline constexpr float kFractalJuliaOrbitCenterX = 0.0f;

inline constexpr float kFractalJuliaOrbitCenterY = 0.0f;

inline constexpr float kFractalAmplitudeSmoothingTauMs = 2000.0f;

inline constexpr float kFractalColorCycleSpeedHz = 0.05f;

inline constexpr float kFractalBoundaryMarginPx = 18.0f;

inline constexpr int kFractalPaletteGradientWidth = 256;

}
