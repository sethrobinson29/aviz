#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include "audio_features.h"
#include "feature_extractor.h"
#include "ring_buffer.h"

// Pure logic pulled out of FeatureWorker's poll loop so it's unit-testable
// without spinning up a thread or an AudioRingBuffer (TESTING.md: pure-logic
// pieces get direct unit tests). `audio_worker_utils` namespace since these
// aren't part of FeatureWorker's public interface, just its internal
// per-tick math, exposed for tests/audio/feature_worker_test.cpp.
namespace audio_worker_utils {

// Downmixes `frame_count` interleaved frames (each `channels` wide) to mono
// by averaging channels. `mono_out` must have room for `frame_count` floats.
void mixdown_to_mono(const float* interleaved, std::size_t frame_count, unsigned channels,
                      float* mono_out);

// Slides a fixed-size window forward: appends `new_samples` (mono, `count`
// of them) at the end, discarding that many samples from the front. If
// `count >= window.size()`, the window becomes just the most recent
// window.size() samples of `new_samples` (worker fell behind by more than a
// full window's worth in one poll).
void slide_window(std::vector<float>& window, const float* new_samples, std::size_t count);

}  // namespace audio_worker_utils

// Feature-extraction worker thread (CLAUDE.md invariant #1). Owns a private
// FeatureExtractor and a sliding mono analysis window; on a fixed-interval
// poll it drains whatever the producer has pushed into the ring buffer,
// slides the window forward, and — if any new audio arrived this tick —
// runs one extraction pass and publishes the result. Single-producer
// (the audio callback via AudioRingBuffer) / single-consumer (this thread)
// on the ring buffer side; single-writer on the AudioFeaturesSnapshot side.
class FeatureWorker {
public:
    // `ring` and `snapshot` must outlive this worker. `sample_rate` is the
    // producer's actual rate (fixed for local file playback per spec §3).
    FeatureWorker(AudioRingBuffer& ring, AudioFeaturesSnapshot& snapshot, unsigned sample_rate);
    ~FeatureWorker();

    FeatureWorker(const FeatureWorker&) = delete;
    FeatureWorker& operator=(const FeatureWorker&) = delete;

    void start();
    void stop();  // idempotent; also called by destructor if still running

private:
    AudioRingBuffer& ring_;
    AudioFeaturesSnapshot& snapshot_;
    unsigned sample_rate_;
    FeatureExtractor extractor_;

    std::vector<float> analysis_window_;   // mono, size kFftWindowSize, sliding
    std::vector<float> interleaved_scratch_;
    std::vector<float> mono_scratch_;

    std::atomic<bool> running_{false};
    std::thread thread_;

    void run();
    std::size_t drain_ring_buffer();  // returns count of new mono samples appended to the window
};
