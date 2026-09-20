#include "app/audio_source_controller.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "audio/audio_config.h"
#include "audio/file_producer.h"
#include "audio/producer_host.h"
#include "audio/ring_buffer.h"

namespace {

std::filesystem::path temp_path(const char* name) { return std::filesystem::temp_directory_path() / name; }

void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

void write_u32le(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 24));
}

void write_u16le(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
}

std::vector<std::uint8_t> make_valid_wav(std::uint32_t sample_rate, std::uint16_t channels,
                                          std::uint32_t num_frames) {
    std::uint32_t data_bytes = num_frames * channels * 2;
    std::vector<std::uint8_t> out;

    out.insert(out.end(), {'R', 'I', 'F', 'F'});
    write_u32le(out, 36 + data_bytes);
    out.insert(out.end(), {'W', 'A', 'V', 'E'});

    out.insert(out.end(), {'f', 'm', 't', ' '});
    write_u32le(out, 16);
    write_u16le(out, 1);
    write_u16le(out, channels);
    write_u32le(out, sample_rate);
    write_u32le(out, sample_rate * channels * 2);
    write_u16le(out, static_cast<std::uint16_t>(channels * 2));
    write_u16le(out, 16);

    out.insert(out.end(), {'d', 'a', 't', 'a'});
    write_u32le(out, data_bytes);
    out.resize(out.size() + data_bytes, 0);
    return out;
}

class AudioSourceControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        wav_path_ = temp_path("aviz_audio_source_controller_test.wav");
        write_bytes(wav_path_, make_valid_wav(44100, 2, 1000));
    }

    void TearDown() override { std::filesystem::remove(wav_path_); }

    std::filesystem::path wav_path_;
};

}

TEST_F(AudioSourceControllerTest, StartsInFileModeNotLoopback) {
    AudioRingBuffer ring(audio_config::kRingBufferCapacityFrames, audio_config::kPipelineChannels);
    ProducerHost host(ring, std::make_unique<FileProducer>(wav_path_.string()));
    AudioSourceController controller(host, wav_path_.string());

    EXPECT_FALSE(controller.is_loopback());
}

TEST_F(AudioSourceControllerTest, ToggleSwitchesToLoopbackThenBackToFile) {
    AudioRingBuffer ring(audio_config::kRingBufferCapacityFrames, audio_config::kPipelineChannels);
    ProducerHost host(ring, std::make_unique<FileProducer>(wav_path_.string()));
    AudioSourceController controller(host, wav_path_.string());

    AudioSourceController::SwitchResult to_loopback = controller.toggle();
    ASSERT_TRUE(to_loopback.success) << to_loopback.error;
    EXPECT_TRUE(controller.is_loopback());
    EXPECT_GT(to_loopback.channels, 0u);
    EXPECT_GT(to_loopback.sample_rate, 0u);

    AudioSourceController::SwitchResult back_to_file = controller.toggle();
    ASSERT_TRUE(back_to_file.success) << back_to_file.error;
    EXPECT_FALSE(controller.is_loopback());
    EXPECT_EQ(back_to_file.channels, 2u);
    EXPECT_EQ(back_to_file.sample_rate, 44100u);
}

TEST_F(AudioSourceControllerTest, RepeatedRapidTogglesLeaveStateConsistentWithTheActualProducer) {
    AudioRingBuffer ring(audio_config::kRingBufferCapacityFrames, audio_config::kPipelineChannels);
    ProducerHost host(ring, std::make_unique<FileProducer>(wav_path_.string()));
    AudioSourceController controller(host, wav_path_.string());

    for (int i = 0; i < 20; ++i) {
        AudioSourceController::SwitchResult result = controller.toggle();
        ASSERT_TRUE(result.success) << result.error;
        EXPECT_EQ(controller.is_loopback(), i % 2 == 0);
    }
}

TEST_F(AudioSourceControllerTest, FailedSwitchToAMissingFileReportsErrorAndPreservesLoopbackState) {
    AudioRingBuffer ring(audio_config::kRingBufferCapacityFrames, audio_config::kPipelineChannels);
    ProducerHost host(ring, std::make_unique<FileProducer>(wav_path_.string()));
    AudioSourceController controller(host, "this_file_does_not_exist.wav");

    AudioSourceController::SwitchResult to_loopback = controller.toggle();
    ASSERT_TRUE(to_loopback.success) << to_loopback.error;
    ASSERT_TRUE(controller.is_loopback());

    AudioSourceController::SwitchResult back_to_file = controller.toggle();
    EXPECT_FALSE(back_to_file.success);
    EXPECT_FALSE(back_to_file.error.empty());
    EXPECT_TRUE(controller.is_loopback());
}
