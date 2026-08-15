#include "audio/feature_worker.h"

#include <gtest/gtest.h>

// FeatureWorker's poll loop drives a real thread against a real
// AudioRingBuffer, but the mixdown/window-slide math it does each tick is
// pure logic — pulled out as audio_worker_utils free functions specifically
// so it's testable here without spinning up either (TESTING.md: pure-logic
// pieces get direct unit tests).

using audio_worker_utils::mixdown_to_mono;
using audio_worker_utils::slide_window;

TEST(MixdownToMonoTest, AveragesStereoChannels) {
    // 3 stereo frames: (2,4), (0,0), (-1,1)
    float interleaved[] = {2.0f, 4.0f, 0.0f, 0.0f, -1.0f, 1.0f};
    float mono[3] = {};

    mixdown_to_mono(interleaved, 3, 2, mono);

    EXPECT_FLOAT_EQ(mono[0], 3.0f);
    EXPECT_FLOAT_EQ(mono[1], 0.0f);
    EXPECT_FLOAT_EQ(mono[2], 0.0f);
}

TEST(MixdownToMonoTest, MonoInputPassesThroughUnchanged) {
    float interleaved[] = {1.0f, -2.0f, 3.5f};
    float mono[3] = {};

    mixdown_to_mono(interleaved, 3, 1, mono);

    EXPECT_FLOAT_EQ(mono[0], 1.0f);
    EXPECT_FLOAT_EQ(mono[1], -2.0f);
    EXPECT_FLOAT_EQ(mono[2], 3.5f);
}

TEST(MixdownToMonoTest, AveragesFourChannels) {
    float interleaved[] = {1.0f, 2.0f, 3.0f, 4.0f};  // one frame, 4 channels
    float mono[1] = {};

    mixdown_to_mono(interleaved, 1, 4, mono);

    EXPECT_FLOAT_EQ(mono[0], 2.5f);
}

TEST(SlideWindowTest, PartialAppendShiftsOldSamplesLeftAndAppendsNew) {
    std::vector<float> window = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    float new_samples[] = {6.0f, 7.0f};

    slide_window(window, new_samples, 2);

    EXPECT_EQ(window, (std::vector<float>{3.0f, 4.0f, 5.0f, 6.0f, 7.0f}));
}

TEST(SlideWindowTest, ZeroCountLeavesWindowUnchanged) {
    std::vector<float> window = {1.0f, 2.0f, 3.0f};
    float new_samples[] = {9.0f};

    slide_window(window, new_samples, 0);

    EXPECT_EQ(window, (std::vector<float>{1.0f, 2.0f, 3.0f}));
}

TEST(SlideWindowTest, CountEqualToWindowSizeReplacesEntireWindow) {
    std::vector<float> window = {1.0f, 2.0f, 3.0f};
    float new_samples[] = {7.0f, 8.0f, 9.0f};

    slide_window(window, new_samples, 3);

    EXPECT_EQ(window, (std::vector<float>{7.0f, 8.0f, 9.0f}));
}

TEST(SlideWindowTest, CountExceedingWindowSizeKeepsOnlyMostRecentTail) {
    std::vector<float> window = {1.0f, 2.0f, 3.0f};  // size 3
    float new_samples[] = {10.0f, 11.0f, 12.0f, 13.0f, 14.0f};  // 5 new samples

    slide_window(window, new_samples, 5);

    // Worker fell behind by more than a window's worth — window becomes
    // just the most recent 3 of the 5 new samples, oldest 2 discarded.
    EXPECT_EQ(window, (std::vector<float>{12.0f, 13.0f, 14.0f}));
}

TEST(SlideWindowTest, RepeatedPartialAppendsProduceCorrectSlidingSequence) {
    std::vector<float> window = {0.0f, 0.0f, 0.0f, 0.0f};

    float batch1[] = {1.0f, 2.0f};
    slide_window(window, batch1, 2);
    EXPECT_EQ(window, (std::vector<float>{0.0f, 0.0f, 1.0f, 2.0f}));

    float batch2[] = {3.0f};
    slide_window(window, batch2, 1);
    EXPECT_EQ(window, (std::vector<float>{0.0f, 1.0f, 2.0f, 3.0f}));

    float batch3[] = {4.0f, 5.0f, 6.0f};
    slide_window(window, batch3, 3);
    EXPECT_EQ(window, (std::vector<float>{3.0f, 4.0f, 5.0f, 6.0f}));
}
