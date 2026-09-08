#include "audio/file_producer.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

std::filesystem::path temp_path(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

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

}

TEST(FileProducerTest, ThrowsOnMissingFile) {
    EXPECT_THROW(FileProducer producer("this_file_does_not_exist.wav"), std::runtime_error);
}

TEST(FileProducerTest, ThrowsOnMalformedFile) {
    auto path = temp_path("aviz_test_malformed.wav");
    write_bytes(path, {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02});

    EXPECT_THROW(FileProducer producer(path.string()), std::runtime_error);

    std::filesystem::remove(path);
}

TEST(FileProducerTest, ThrowsOnTruncatedWav) {
    auto full = make_valid_wav(44100, 2, 1000);
    std::vector<std::uint8_t> truncated(full.begin(), full.begin() + 60);

    auto path = temp_path("aviz_test_truncated.wav");
    write_bytes(path, truncated);

    try {
        FileProducer producer(path.string());
        EXPECT_EQ(producer.native_channels(), 2u);
        EXPECT_EQ(producer.native_sample_rate(), 44100u);
        EXPECT_FALSE(producer.is_finished());
    } catch (const std::runtime_error&) {
    }

    std::filesystem::remove(path);
}

TEST(FileProducerTest, OpensValidFileAndReportsFormat) {
    auto bytes = make_valid_wav(44100, 2, 4410);
    auto path = temp_path("aviz_test_valid.wav");
    write_bytes(path, bytes);

    {
        FileProducer producer(path.string());
        EXPECT_EQ(producer.native_channels(), 2u);
        EXPECT_EQ(producer.native_sample_rate(), 44100u);
        EXPECT_FALSE(producer.is_finished());
    }

    std::filesystem::remove(path);
}
