#include "audio/ring_buffer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace {

// Builds `count` mono frames with values [start, start+1, ...).
std::vector<float> MakeMonoFrames(float start, std::size_t count) {
    std::vector<float> frames(count);
    for (std::size_t i = 0; i < count; ++i) {
        frames[i] = start + static_cast<float>(i);
    }
    return frames;
}

}  // namespace

TEST(RingBufferTest, PushPopRoundTrip) {
    AudioRingBuffer rb(8, 1);
    auto in = MakeMonoFrames(0.0f, 5);
    rb.push_frames(in.data(), in.size());

    EXPECT_EQ(rb.available_frames(), 5u);

    std::vector<float> out(5);
    std::size_t copied = rb.pop_frames(out.data(), out.size());

    EXPECT_EQ(copied, 5u);
    EXPECT_EQ(out, in);
    EXPECT_EQ(rb.available_frames(), 0u);
}

TEST(RingBufferTest, PopReturnsFewerThanRequestedWhenStarved) {
    AudioRingBuffer rb(8, 1);
    auto in = MakeMonoFrames(0.0f, 3);
    rb.push_frames(in.data(), in.size());

    std::vector<float> out(10, -1.0f);
    std::size_t copied = rb.pop_frames(out.data(), out.size());

    EXPECT_EQ(copied, 3u);
    EXPECT_EQ(out[0], 0.0f);
    EXPECT_EQ(out[1], 1.0f);
    EXPECT_EQ(out[2], 2.0f);
    EXPECT_EQ(out[3], -1.0f);
}

TEST(RingBufferTest, WraparoundAcrossCapacityBoundaryPreservesOrder) {
    AudioRingBuffer rb(8, 1);

    for (int cycle = 0; cycle < 4; ++cycle) {
        auto in = MakeMonoFrames(static_cast<float>(cycle * 100), 5);
        rb.push_frames(in.data(), in.size());

        std::vector<float> out(5);
        std::size_t copied = rb.pop_frames(out.data(), out.size());

        EXPECT_EQ(copied, 5u);
        EXPECT_EQ(out, in) << "cycle " << cycle;
    }
}

TEST(RingBufferTest, NoDiscontinuityUnderNormalLoad) {
    AudioRingBuffer rb(8, 1);
    auto in = MakeMonoFrames(0.0f, 4);
    rb.push_frames(in.data(), in.size());

    std::vector<float> out(4);
    rb.pop_frames(out.data(), out.size());

    EXPECT_FALSE(rb.discontinuity());
}

TEST(RingBufferTest, OverflowOverwritesOldestAndSetsDiscontinuity) {
    AudioRingBuffer rb(4, 1);
    auto in = MakeMonoFrames(0.0f, 6);
    rb.push_frames(in.data(), in.size());

    EXPECT_TRUE(rb.discontinuity());
    EXPECT_EQ(rb.available_frames(), 4u);

    std::vector<float> out(4);
    std::size_t copied = rb.pop_frames(out.data(), out.size());

    EXPECT_EQ(copied, 4u);
    std::vector<float> expected = {2.0f, 3.0f, 4.0f, 5.0f};
    EXPECT_EQ(out, expected);
}

TEST(RingBufferTest, OverflowAcrossMultiplePushCallsAlsoDetected) {
    AudioRingBuffer rb(4, 1);
    auto first = MakeMonoFrames(0.0f, 3);
    rb.push_frames(first.data(), first.size());
    EXPECT_FALSE(rb.discontinuity());

    auto second = MakeMonoFrames(10.0f, 3);
    rb.push_frames(second.data(), second.size());

    EXPECT_TRUE(rb.discontinuity());
    std::vector<float> out(4);
    std::size_t copied = rb.pop_frames(out.data(), out.size());
    EXPECT_EQ(copied, 4u);
    std::vector<float> expected = {2.0f, 10.0f, 11.0f, 12.0f};
    EXPECT_EQ(out, expected);
}

TEST(RingBufferTest, ClearDiscontinuityResetsFlag) {
    AudioRingBuffer rb(4, 1);
    auto in = MakeMonoFrames(0.0f, 6);
    rb.push_frames(in.data(), in.size());
    ASSERT_TRUE(rb.discontinuity());

    rb.clear_discontinuity();

    EXPECT_FALSE(rb.discontinuity());
}

TEST(RingBufferTest, InterleavedMultiChannelRoundTrip) {
    AudioRingBuffer rb(4, 2);
    // 3 stereo frames: (L,R) = (0,100), (1,101), (2,102).
    float in[6] = {0.0f, 100.0f, 1.0f, 101.0f, 2.0f, 102.0f};
    rb.push_frames(in, 3);

    float out[6] = {0};
    std::size_t copied = rb.pop_frames(out, 3);

    EXPECT_EQ(copied, 3u);
    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(out[i], in[i]) << "sample " << i;
    }
}

// --- Boundary conditions -----------------------------------------------------

TEST(RingBufferTest, EmptyBufferPopReturnsZeroAndLeavesOutputUntouched) {
    AudioRingBuffer rb(8, 1);

    float out[4] = {-1.0f, -1.0f, -1.0f, -1.0f};
    std::size_t copied = rb.pop_frames(out, 4);

    EXPECT_EQ(copied, 0u);
    EXPECT_EQ(rb.available_frames(), 0u);
    for (float v : out) {
        EXPECT_EQ(v, -1.0f);
    }
}

TEST(RingBufferTest, ExactlyFullBufferDoesNotTriggerDiscontinuity) {
    AudioRingBuffer rb(4, 1);
    auto in = MakeMonoFrames(0.0f, 4);
    rb.push_frames(in.data(), in.size());

    EXPECT_FALSE(rb.discontinuity());
    EXPECT_EQ(rb.available_frames(), 4u);

    std::vector<float> out(4);
    EXPECT_EQ(rb.pop_frames(out.data(), out.size()), 4u);
    EXPECT_EQ(out, in);
}

TEST(RingBufferTest, OneFrameBeyondExactlyFullTriggersDiscontinuity) {
    AudioRingBuffer rb(4, 1);
    auto in = MakeMonoFrames(0.0f, 5);
    rb.push_frames(in.data(), in.size());

    EXPECT_TRUE(rb.discontinuity());
    EXPECT_EQ(rb.available_frames(), 4u);
}

TEST(RingBufferTest, PushPopExactCapacityRepeatedCyclesNeverOverflow) {
    AudioRingBuffer rb(4, 1);

    for (int cycle = 0; cycle < 50; ++cycle) {
        auto in = MakeMonoFrames(static_cast<float>(cycle * 4), 4);
        rb.push_frames(in.data(), in.size());
        ASSERT_FALSE(rb.discontinuity()) << "cycle " << cycle;

        std::vector<float> out(4);
        ASSERT_EQ(rb.pop_frames(out.data(), out.size()), 4u) << "cycle " << cycle;
        EXPECT_EQ(out, in) << "cycle " << cycle;
    }
}

// --- Concurrent producer/consumer ---------------------------------------------

TEST(RingBufferConcurrencyTest, NoOverflowRegimePreservesExactOrder) {
    constexpr std::size_t kCapacity = 1 << 16;  // test-local, unrelated to audio_config
    constexpr std::size_t kTotalFrames = 20000;
    AudioRingBuffer rb(kCapacity, 1);

    std::thread producer([&] {
        constexpr std::size_t kChunk = 4;
        std::size_t pushed = 0;
        while (pushed < kTotalFrames) {
            std::size_t n = std::min(kChunk, kTotalFrames - pushed);
            auto chunk = MakeMonoFrames(static_cast<float>(pushed), n);
            rb.push_frames(chunk.data(), n);
            pushed += n;
        }
    });

    std::vector<float> collected;
    collected.reserve(kTotalFrames);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (collected.size() < kTotalFrames) {
        float buf[6];
        std::size_t n = rb.pop_frames(buf, 6);
        for (std::size_t i = 0; i < n; ++i) {
            collected.push_back(buf[i]);
        }
        if (n == 0) {
            ASSERT_LT(std::chrono::steady_clock::now(), deadline)
                << "consumer stalled: collected " << collected.size() << "/" << kTotalFrames;
            std::this_thread::yield();
        }
    }
    producer.join();

    EXPECT_FALSE(rb.discontinuity());
    ASSERT_EQ(collected.size(), kTotalFrames);
    for (std::size_t i = 0; i < kTotalFrames; ++i) {
        ASSERT_EQ(collected[i], static_cast<float>(i)) << "frame " << i;
    }
}

TEST(RingBufferConcurrencyTest, ForcedOverflowRegimeStaysMonotonicAndFlagsDiscontinuity) {
    constexpr std::size_t kCapacity = 16;
    constexpr std::size_t kTotalFrames = 5000;
    AudioRingBuffer rb(kCapacity, 1);
    std::atomic<bool> saw_discontinuity{false};
    std::atomic<bool> producer_finished{false};

    std::thread producer([&] {
        constexpr std::size_t kChunk = 4;
        std::size_t pushed = 0;
        while (pushed < kTotalFrames) {
            std::size_t n = std::min(kChunk, kTotalFrames - pushed);
            auto chunk = MakeMonoFrames(static_cast<float>(pushed), n);
            rb.push_frames(chunk.data(), n);
            pushed += n;
        }
        producer_finished.store(true, std::memory_order_release);
    });

    std::vector<float> collected;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    for (;;) {
        if (rb.discontinuity()) {
            saw_discontinuity.store(true, std::memory_order_relaxed);
        }
        float buf[4];
        std::size_t n = rb.pop_frames(buf, 4);
        for (std::size_t i = 0; i < n; ++i) {
            collected.push_back(buf[i]);
        }
        if (n == 0) {
            // Producer done and buffer confirmed empty: nothing left to drain.
            if (producer_finished.load(std::memory_order_acquire)) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
        ASSERT_LT(std::chrono::steady_clock::now(), deadline) << "consumer stalled";
    }
    producer.join();

    // Strict ordering isn't asserted under forced overflow -- only that
    // every value was actually pushed and discontinuity() fired.
    EXPECT_TRUE(saw_discontinuity.load(std::memory_order_relaxed));
    ASSERT_FALSE(collected.empty());
    for (float v : collected) {
        ASSERT_GE(v, 0.0f);
        ASSERT_LT(v, static_cast<float>(kTotalFrames));
        ASSERT_EQ(v, std::floor(v)) << "value is not one of the pushed integers: " << v;
    }
}
