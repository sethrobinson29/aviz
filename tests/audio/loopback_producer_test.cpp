#include "audio/loopback_producer.h"

#include <gtest/gtest.h>

#include "audio/audio_config.h"
#include "audio/ring_buffer.h"

// Requires a real WASAPI-capable output device -- per TESTING.md's
// No-hardware-fallback section, this project's dev/test environment always
// has one. These tests cover device lifecycle only; captured-audio
// correctness is human-review territory (see milestone-4-debug-controls.md
// and TESTING.md's Audio correctness section) since there is no synthetic
// loopback signal to assert against.

TEST(LoopbackProducerTest, OpensDefaultDeviceAndReportsSaneFormat) {
    LoopbackProducer producer;

    // Real consumer audio devices: a handful of channels, tens of kHz. These
    // bounds are chosen to actually distinguish correct wiring from a
    // channels/sample_rate swap bug (a swap would put a several-thousand
    // value where a small channel count is expected, and vice versa) --
    // EXPECT_GT(..., 0u) alone can't tell the two getters apart.
    EXPECT_GT(producer.native_channels(), 0u);
    EXPECT_LE(producer.native_channels(), 8u);
    EXPECT_GE(producer.native_sample_rate(), 8000u);
    EXPECT_LE(producer.native_sample_rate(), 192000u);
}

TEST(LoopbackProducerTest, StartAndStopDoNotCrash) {
    LoopbackProducer producer;
    AudioRingBuffer ring(audio_config::kRingBufferCapacityFrames, audio_config::kPipelineChannels);

    producer.start(ring);
    producer.stop();
}
