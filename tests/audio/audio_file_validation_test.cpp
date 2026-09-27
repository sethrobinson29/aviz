#include "audio/audio_file_validation.h"

#include <gtest/gtest.h>

TEST(AudioFileValidationTest, AcceptsWavPath) {
    EXPECT_TRUE(audio_file_validation::is_plausible_audio_file_path("C:\\music\\track.wav"));
}

TEST(AudioFileValidationTest, AcceptsMp3PathCaseInsensitive) {
    EXPECT_TRUE(audio_file_validation::is_plausible_audio_file_path("C:\\music\\track.MP3"));
}

TEST(AudioFileValidationTest, AcceptsFlacPath) {
    EXPECT_TRUE(audio_file_validation::is_plausible_audio_file_path("track.flac"));
}

TEST(AudioFileValidationTest, RejectsEmptyPath) {
    EXPECT_FALSE(audio_file_validation::is_plausible_audio_file_path(""));
}

TEST(AudioFileValidationTest, RejectsUnsupportedExtension) {
    EXPECT_FALSE(audio_file_validation::is_plausible_audio_file_path("track.exe"));
    EXPECT_FALSE(audio_file_validation::is_plausible_audio_file_path("track.txt"));
}

TEST(AudioFileValidationTest, RejectsNoExtension) {
    EXPECT_FALSE(audio_file_validation::is_plausible_audio_file_path("track"));
}

TEST(AudioFileValidationTest, RejectsEmbeddedControlCharacter) {
    std::string path = "track.wav";
    path.insert(path.begin(), '\n');
    EXPECT_FALSE(audio_file_validation::is_plausible_audio_file_path(path));
}

TEST(AudioFileValidationTest, RejectsPathShorterThanExtension) {
    EXPECT_FALSE(audio_file_validation::is_plausible_audio_file_path(".wa"));
}
