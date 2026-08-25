#include "audio/pipeline_format_converter.h"

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "audio/audio_config.h"
#include "audio/ring_buffer.h"

TEST(PipelineFormatConverterTest, IdentityFormatPassesSamplesThroughUnchanged) {
    PipelineFormatConverter converter(audio_config::kPipelineChannels, audio_config::kPipelineSampleRate);
    AudioRingBuffer ring(1 << 14, audio_config::kPipelineChannels);

    constexpr std::size_t kFrames = 1000;
    std::vector<float> input(kFrames * audio_config::kPipelineChannels);
    for (std::size_t i = 0; i < input.size(); ++i) {
        input[i] = static_cast<float>(i % 97) / 97.0f - 0.5f;  // distinct per-sample values
    }

    converter.convert_and_push(input.data(), kFrames, ring);

    ASSERT_EQ(ring.available_frames(), kFrames);

    std::vector<float> out(kFrames * audio_config::kPipelineChannels);
    ring.pop_frames(out.data(), kFrames);
    for (std::size_t i = 0; i < out.size(); ++i) {
        EXPECT_NEAR(out[i], input[i], 1e-5f) << "sample " << i;
    }
}

TEST(PipelineFormatConverterTest, MonoInputUpmixesToStereoChannelCount) {
    PipelineFormatConverter converter(1, audio_config::kPipelineSampleRate);
    AudioRingBuffer ring(1 << 14, audio_config::kPipelineChannels);

    constexpr std::size_t kFrames = 500;
    std::vector<float> input(kFrames, 0.25f);

    converter.convert_and_push(input.data(), kFrames, ring);

    ASSERT_EQ(ring.available_frames(), kFrames);

    std::vector<float> out(kFrames * audio_config::kPipelineChannels);
    ring.pop_frames(out.data(), kFrames);
    for (std::size_t i = 0; i < kFrames; ++i) {
        EXPECT_NEAR(out[i * 2], 0.25f, 1e-4f);
        EXPECT_NEAR(out[i * 2 + 1], 0.25f, 1e-4f);
    }
}

TEST(PipelineFormatConverterTest, UpsamplingProducesApproximatelyExpectedFrameCount) {
    constexpr unsigned kSourceRate = 44100;  // < kPipelineSampleRate (48000)
    PipelineFormatConverter converter(audio_config::kPipelineChannels, kSourceRate);
    AudioRingBuffer ring(1 << 17, audio_config::kPipelineChannels);

    constexpr std::size_t kFrames = 44100;  // 1 second at source rate
    std::vector<float> input(kFrames * audio_config::kPipelineChannels, 0.1f);

    converter.convert_and_push(input.data(), kFrames, ring);

    double expected =
        static_cast<double>(kFrames) * audio_config::kPipelineSampleRate / static_cast<double>(kSourceRate);
    EXPECT_NEAR(static_cast<double>(ring.available_frames()), expected, 50.0);
}

TEST(PipelineFormatConverterTest, DownsamplingProducesApproximatelyExpectedFrameCount) {
    constexpr unsigned kSourceRate = 96000;  // > kPipelineSampleRate (48000)
    PipelineFormatConverter converter(audio_config::kPipelineChannels, kSourceRate);
    AudioRingBuffer ring(1 << 17, audio_config::kPipelineChannels);

    constexpr std::size_t kFrames = 96000;  // 1 second at source rate
    std::vector<float> input(kFrames * audio_config::kPipelineChannels, 0.1f);

    converter.convert_and_push(input.data(), kFrames, ring);

    double expected =
        static_cast<double>(kFrames) * audio_config::kPipelineSampleRate / static_cast<double>(kSourceRate);
    EXPECT_NEAR(static_cast<double>(ring.available_frames()), expected, 50.0);
    EXPECT_LT(ring.available_frames(), kFrames);  // confirms downsampling actually reduced the count
}
