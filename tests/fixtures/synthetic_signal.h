#pragma once

#include <cmath>
#include <cstddef>
#include <random>
#include <vector>

// Synthetic mono signal generators for feeding FeatureExtractor::process()
// directly in unit tests.
namespace fixtures {

inline std::vector<float> sine_wave(float freq_hz, unsigned sample_rate, std::size_t num_samples,
                                     float amplitude = 1.0f) {
    constexpr float kPi = 3.14159265358979323846f;
    std::vector<float> out(num_samples);
    for (std::size_t i = 0; i < num_samples; ++i) {
        out[i] = amplitude * std::sin(2.0f * kPi * freq_hz * i / sample_rate);
    }
    return out;
}

inline std::vector<float> silence(std::size_t num_samples) {
    return std::vector<float>(num_samples, 0.0f);
}

// A short burst of `burst_amplitude` every `period_samples`, `burst_len`
// samples wide, silence otherwise.
inline std::vector<float> impulse_train(std::size_t num_samples, std::size_t period_samples,
                                         std::size_t burst_len = 4, float burst_amplitude = 1.0f) {
    std::vector<float> out(num_samples, 0.0f);
    for (std::size_t start = 0; start < num_samples; start += period_samples) {
        for (std::size_t i = 0; i < burst_len && start + i < num_samples; ++i) {
            out[start + i] = burst_amplitude;
        }
    }
    return out;
}

inline std::vector<float> white_noise(std::size_t num_samples, float amplitude = 1.0f,
                                       unsigned seed = 1234) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-amplitude, amplitude);
    std::vector<float> out(num_samples);
    for (std::size_t i = 0; i < num_samples; ++i) {
        out[i] = dist(rng);
    }
    return out;
}

}  // namespace fixtures
