#include "audio/audio_features.h"

#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

TEST(AudioFeaturesSnapshotTest, InitialCurrentIsDefaultConstructed) {
    AudioFeaturesSnapshot snap;

    EXPECT_EQ(snap.current().rms, 0.0f);
    EXPECT_EQ(snap.current().peak_frequency_hz, 0.0f);
}

TEST(AudioFeaturesSnapshotTest, BackBufferWritesNotVisibleBeforePublish) {
    AudioFeaturesSnapshot snap;

    snap.back_buffer().rms = 42.0f;

    EXPECT_EQ(snap.current().rms, 0.0f);
}

TEST(AudioFeaturesSnapshotTest, PublishMakesWriteVisible) {
    AudioFeaturesSnapshot snap;

    snap.back_buffer().rms = 42.0f;
    snap.publish();

    EXPECT_EQ(snap.current().rms, 42.0f);
}

TEST(AudioFeaturesSnapshotTest, AlternatesBuffersAcrossPublishCyclesWithoutClobbering) {
    AudioFeaturesSnapshot snap;

    snap.back_buffer().rms = 1.0f;
    snap.publish();
    EXPECT_EQ(snap.current().rms, 1.0f);

    // Writing into the new back buffer must not disturb what's published.
    snap.back_buffer().rms = 2.0f;
    EXPECT_EQ(snap.current().rms, 1.0f);

    snap.publish();
    EXPECT_EQ(snap.current().rms, 2.0f);

    snap.back_buffer().rms = 3.0f;
    EXPECT_EQ(snap.current().rms, 2.0f);

    snap.publish();
    EXPECT_EQ(snap.current().rms, 3.0f);
}

TEST(AudioFeaturesSnapshotTest, BandArraysCarryThroughPublish) {
    AudioFeaturesSnapshot snap;

    AudioFeatures& back = snap.back_buffer();
    back.band_energy[audio_config::kGlobalBandIndex] = 0.75f;
    back.band_onset[audio_config::kGlobalBandIndex] = true;
    back.band_energy[0] = 0.25f;

    snap.publish();

    const AudioFeatures& current = snap.current();
    EXPECT_FLOAT_EQ(current.band_energy[audio_config::kGlobalBandIndex], 0.75f);
    EXPECT_TRUE(current.band_onset[audio_config::kGlobalBandIndex]);
    EXPECT_FLOAT_EQ(current.band_energy[0], 0.25f);
}

// --- Concurrent worker/render ------------------------------------------------
//
// Mirrors ring_buffer_test.cpp's concurrency tests: a real worker thread and
// a real render thread against one AudioFeaturesSnapshot, not just sequential
// back_buffer()/publish()/current() calls from one thread. This is the same
// pattern that caught a genuine data race in the ring buffer, so it's applied
// at full strength here rather than a lighter spot-check.
//
// Each published AudioFeatures carries the same sequence number written into
// six different fields spread across the struct (rms, peak_frequency_hz,
// band_energy[0], band_energy[kGlobalBandIndex], waveform.front(),
// waveform.back()). A reader observing anything other than all six fields
// agreeing would mean it read a torn mix of two different publishes —
// exactly the failure mode the ring buffer test caught.
TEST(AudioFeaturesSnapshotConcurrencyTest, RenderNeverObservesTornOrOutOfOrderPublish) {
    constexpr int kIterations = 200000;
    AudioFeaturesSnapshot snap;
    std::atomic<bool> worker_finished{false};

    std::thread worker([&] {
        for (int seq = 1; seq <= kIterations; ++seq) {
            AudioFeatures& f = snap.back_buffer();
            float v = static_cast<float>(seq);
            f.rms = v;
            f.peak_frequency_hz = v;
            f.band_energy[0] = v;
            f.band_energy[audio_config::kGlobalBandIndex] = v;
            f.waveform.front() = v;
            f.waveform.back() = v;
            snap.publish();
        }
        worker_finished.store(true, std::memory_order_release);
    });

    float last_seen = 0.0f;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (;;) {
        const AudioFeatures& f = snap.current();
        float a = f.rms;
        float b = f.peak_frequency_hz;
        float c = f.band_energy[0];
        float d = f.band_energy[audio_config::kGlobalBandIndex];
        float e = f.waveform.front();
        float g = f.waveform.back();

        ASSERT_EQ(a, b) << "rms/peak_frequency_hz disagree: torn publish";
        ASSERT_EQ(a, c) << "rms/band_energy[0] disagree: torn publish";
        ASSERT_EQ(a, d) << "rms/band_energy[global] disagree: torn publish";
        ASSERT_EQ(a, e) << "rms/waveform.front() disagree: torn publish";
        ASSERT_EQ(a, g) << "rms/waveform.back() disagree: torn publish";
        ASSERT_GE(a, last_seen) << "sequence number went backward: torn or reordered publish";
        last_seen = a;

        if (worker_finished.load(std::memory_order_acquire) &&
            last_seen == static_cast<float>(kIterations)) {
            break;
        }
        ASSERT_LT(std::chrono::steady_clock::now(), deadline)
            << "render thread never observed the final publish (last seen: " << last_seen << ")";
    }
    worker.join();

    EXPECT_EQ(last_seen, static_cast<float>(kIterations));
}
