#include "feature_worker.h"

#include <algorithm>
#include <chrono>

namespace {
constexpr std::size_t kScratchChunkFrames = audio_config::kFftHopSize * 2;
}

namespace audio_worker_utils {

void mixdown_to_mono(const float* interleaved, std::size_t frame_count, unsigned channels,
                      float* mono_out) {
    for (std::size_t i = 0; i < frame_count; ++i) {
        float sum = 0.0f;
        for (unsigned c = 0; c < channels; ++c) {
            sum += interleaved[i * channels + c];
        }
        mono_out[i] = sum / static_cast<float>(channels);
    }
}

void slide_window(std::vector<float>& window, const float* new_samples, std::size_t count) {
    if (count == 0) {
        return;
    }
    if (count >= window.size()) {
        std::copy(new_samples + (count - window.size()), new_samples + count, window.begin());
    } else {
        std::copy(window.begin() + count, window.end(), window.begin());
        std::copy(new_samples, new_samples + count, window.end() - count);
    }
}

}  // namespace audio_worker_utils

FeatureWorker::FeatureWorker(AudioRingBuffer& ring, AudioFeaturesSnapshot& snapshot,
                              unsigned sample_rate)
    : ring_(ring),
      snapshot_(snapshot),
      sample_rate_(sample_rate),
      extractor_(sample_rate),
      analysis_window_(audio_config::kFftWindowSize, 0.0f),
      interleaved_scratch_(kScratchChunkFrames * ring.channels()),
      mono_scratch_(kScratchChunkFrames) {}

FeatureWorker::~FeatureWorker() { stop(); }

void FeatureWorker::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&FeatureWorker::run, this);
}

void FeatureWorker::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
}

std::size_t FeatureWorker::drain_ring_buffer() {
    unsigned channels = ring_.channels();
    std::size_t total = 0;

    for (;;) {
        std::size_t popped = ring_.pop_frames(interleaved_scratch_.data(), kScratchChunkFrames);
        if (popped == 0) {
            break;
        }
        audio_worker_utils::mixdown_to_mono(interleaved_scratch_.data(), popped, channels,
                                             mono_scratch_.data());
        audio_worker_utils::slide_window(analysis_window_, mono_scratch_.data(), popped);
        total += popped;

        if (popped < kScratchChunkFrames) {
            break;
        }
    }
    return total;
}

void FeatureWorker::run() {
    auto interval = std::chrono::duration<double>(
        static_cast<double>(audio_config::kFftHopSize) / sample_rate_ /
        audio_config::kWorkerPollIntervalDivisor);

    auto next_wake = std::chrono::steady_clock::now();
    while (running_.load(std::memory_order_relaxed)) {
        next_wake += std::chrono::duration_cast<std::chrono::steady_clock::duration>(interval);

        std::size_t new_samples = drain_ring_buffer();
        bool had_discontinuity = ring_.discontinuity();
        if (had_discontinuity) {
            ring_.clear_discontinuity();
        }

        if (new_samples > 0) {
            AudioFeatures& back = snapshot_.back_buffer();
            extractor_.process(analysis_window_.data(), had_discontinuity, back);
            snapshot_.publish();
        }

        std::this_thread::sleep_until(next_wake);
    }
}
