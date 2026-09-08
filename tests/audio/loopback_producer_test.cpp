#include "audio/loopback_producer.h"

#include <gtest/gtest.h>

#include "audio/audio_config.h"
#include "audio/ring_buffer.h"

TEST(LoopbackProducerTest, OpensDefaultDeviceAndReportsSaneFormat) {
    LoopbackProducer producer;

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
