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

inline constexpr int kParticleSpriteSizePx = 16;

inline constexpr int kParticleDefaultLifetimeFrames = 150;

inline constexpr float kParticlesSpawnRatePerFrameMin = 0.0f;

inline constexpr float kParticlesSpawnRatePerFrameMax = 20.0f;

inline constexpr int kParticlesLifetimeFramesMin = 10;

inline constexpr int kParticlesLifetimeFramesMax = 600;

inline constexpr int kFlowFieldGridCols = 32;

inline constexpr int kFlowFieldGridRows = 18;

inline constexpr int kFlowFieldMaxGridCells = kFlowFieldGridCols * kFlowFieldGridRows;

inline constexpr int kFlowFieldGridColsMin = 8;

inline constexpr int kFlowFieldGridColsMax = kFlowFieldGridCols;

inline constexpr float kFlowFieldBaseTurbulenceFrequency = 0.01f;

inline constexpr float kFlowFieldBaseEvolutionPerFrame = 0.01f;

inline constexpr float kFlowFieldTurbulenceFrequencyMin = 0.002f;

inline constexpr float kFlowFieldTurbulenceFrequencyMax = 0.05f;

inline constexpr float kFlowFieldEvolutionPerFrameMin = 0.0f;

inline constexpr float kFlowFieldEvolutionPerFrameMax = 0.05f;

inline constexpr float kFlowFieldSteerFactor = 0.15f;

inline constexpr float kFlowFieldSpeedPxPerFrame = 2.0f;

inline constexpr int kFlowFieldDefaultLifetimeFrames = 400;

inline constexpr float kFlowFieldDefaultSpawnRatePerFrame = 0.75f;

inline constexpr int kFlowFieldDefaultBurstCount = 16;

inline constexpr float kFlowFieldSpawnRatePerFrameMin = 0.0f;

inline constexpr float kFlowFieldSpawnRatePerFrameMax = 5.0f;

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

inline constexpr float kFractalBaseZoomMin = 1.0f;

inline constexpr float kFractalBaseZoomMax = 4.0f;

inline constexpr float kFractalZoomAmplitudeScaleMin = 0.0f;

inline constexpr float kFractalZoomAmplitudeScaleMax = 2.0f;

inline constexpr float kFractalJuliaBaseRadiusMin = 0.70f;

inline constexpr float kFractalJuliaBaseRadiusMax = 0.85f;

inline constexpr float kFractalJuliaRadiusAmplitudeScaleMin = 0.0f;

inline constexpr float kFractalJuliaRadiusAmplitudeScaleMax = 0.2f;

inline constexpr float kFractalJuliaAngularSpeedRadiansPerSecMin = 0.0f;

inline constexpr float kFractalJuliaAngularSpeedRadiansPerSecMax = 1.0f;

inline constexpr float kFractalJuliaOrbitCenterMin = -0.5f;

inline constexpr float kFractalJuliaOrbitCenterMax = 0.5f;

inline constexpr float kFractalAmplitudeSmoothingTauMs = 2000.0f;

inline constexpr float kFractalColorCycleSpeedHz = 0.05f;

inline constexpr float kFractalColorCycleSpeedHzMin = 0.0f;

inline constexpr float kFractalColorCycleSpeedHzMax = 0.5f;

inline constexpr float kFractalBoundaryMarginPx = 18.0f;

inline constexpr int kFractalPaletteGradientWidth = 256;

inline constexpr float kLissajousCompressionExponentMin = 0.2f;

inline constexpr float kLissajousCompressionExponentMax = 1.0f;

}
