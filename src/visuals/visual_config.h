#pragma once

// Tunable constants shared across visual modules.
namespace visual_config {

// Below this RMS, a visual module renders its flat/idle rest pose instead
// of near-silence noise.
inline constexpr float kIdleRmsFloor = 0.01f;

}  // namespace visual_config
