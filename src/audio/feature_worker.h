#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include "audio_features.h"
#include "feature_extractor.h"
#include "ring_buffer.h"

namespace audio_worker_utils {

void mixdown_to_mono(const float* interleaved, std::size_t frame_count, unsigned channels,
                      float* mono_out);

void slide_window(std::vector<float>& window, const float* new_samples, std::size_t count);

}

class FeatureWorker {
public:
    FeatureWorker(AudioRingBuffer& ring, AudioFeaturesSnapshot& snapshot, unsigned sample_rate);
    ~FeatureWorker();

    FeatureWorker(const FeatureWorker&) = delete;
    FeatureWorker& operator=(const FeatureWorker&) = delete;

    void start();
    void stop();

private:
    AudioRingBuffer& ring_;
    AudioFeaturesSnapshot& snapshot_;
    unsigned sample_rate_;
    FeatureExtractor extractor_;

    std::vector<float> analysis_window_;
    std::vector<float> interleaved_scratch_;
    std::vector<float> mono_scratch_;

    std::atomic<bool> running_{false};
    std::thread thread_;

    void run();
    std::size_t drain_ring_buffer();
};
