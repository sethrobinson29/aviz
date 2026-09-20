#include "compositor/preset_file_io.h"

#include <filesystem>

#include <gtest/gtest.h>

namespace {

class PresetFileIoTest : public ::testing::Test {
protected:
    void SetUp() override {
        scratch_dir_ = std::filesystem::temp_directory_path() / "aviz_preset_file_io_test";
        std::filesystem::remove_all(scratch_dir_);
    }

    void TearDown() override { std::filesystem::remove_all(scratch_dir_); }

    std::filesystem::path scratch_dir_;
};

class PresetDirectoryListTest : public ::testing::Test {
protected:
    void SetUp() override { std::filesystem::remove_all(preset_file_io::kPresetsDirectoryName); }

    void TearDown() override { std::filesystem::remove_all(preset_file_io::kPresetsDirectoryName); }
};

}

TEST_F(PresetFileIoTest, WriteThenReadRoundTripsExactContents) {
    const std::string path = (scratch_dir_ / "roundtrip.json").string();
    const std::string contents = "{\"schema_version\":1,\"layers\":[]}";

    preset_file_io::WriteResult write_result = preset_file_io::write_file(path, contents);
    ASSERT_TRUE(write_result.success) << write_result.error;

    preset_file_io::ReadResult read_result = preset_file_io::read_file(path);
    ASSERT_TRUE(read_result.success) << read_result.error;
    EXPECT_EQ(read_result.contents, contents);
}

TEST_F(PresetFileIoTest, WriteCreatesMissingParentDirectory) {
    ASSERT_FALSE(std::filesystem::exists(scratch_dir_));
    const std::string path = (scratch_dir_ / "nested" / "preset.json").string();

    preset_file_io::WriteResult write_result = preset_file_io::write_file(path, "content");

    EXPECT_TRUE(write_result.success) << write_result.error;
    EXPECT_TRUE(std::filesystem::exists(path));
}

TEST_F(PresetFileIoTest, WriteOverwritesExistingFile) {
    const std::string path = (scratch_dir_ / "overwrite.json").string();
    ASSERT_TRUE(preset_file_io::write_file(path, "first").success);

    preset_file_io::WriteResult second = preset_file_io::write_file(path, "second-and-shorter");

    ASSERT_TRUE(second.success);
    EXPECT_EQ(preset_file_io::read_file(path).contents, "second-and-shorter");
}

TEST_F(PresetFileIoTest, ReadOnMissingFileReturnsClearFailureNotAnException) {
    const std::string path = (scratch_dir_ / "does_not_exist.json").string();

    preset_file_io::ReadResult result = preset_file_io::read_file(path);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST_F(PresetFileIoTest, ReadOnFileWithinAMissingDirectoryReturnsClearFailure) {
    const std::string path = (scratch_dir_ / "no_such_dir" / "preset.json").string();

    preset_file_io::ReadResult result = preset_file_io::read_file(path);

    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error.empty());
}

TEST(IsSafePresetFilenameTest, AcceptsOrdinaryJsonFilename) {
    EXPECT_TRUE(preset_file_io::is_safe_preset_filename("my-preset.json"));
}

TEST(IsSafePresetFilenameTest, RejectsEmpty) {
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename(""));
}

TEST(IsSafePresetFilenameTest, RejectsDotAndDotDot) {
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("."));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename(".."));
}

TEST(IsSafePresetFilenameTest, RejectsPathTraversal) {
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("../secrets.json"));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("..\\secrets.json"));
}

TEST(IsSafePresetFilenameTest, RejectsForwardAndBackSlashes) {
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("sub/dir.json"));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("sub\\dir.json"));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("/etc/passwd.json"));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("C:\\Windows\\x.json"));
}

TEST(IsSafePresetFilenameTest, RejectsMissingJsonExtension) {
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("preset"));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("preset.txt"));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename(".json"));
}

TEST(IsSafePresetFilenameTest, RejectsControlCharacters) {
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename(std::string("bad\0name.json", 13)));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("bad\nname.json"));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("bad\tname.json"));
}

TEST(IsSafePresetFilenameTest, RejectsOverlyLongFilename) {
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename(std::string(196, 'a') + ".json"));
}

TEST(PresetPathTest, JoinsPresetsDirectoryAndFilename) {
    const std::string joined = preset_file_io::preset_path("debug.json");
    EXPECT_EQ(std::filesystem::path(joined),
              std::filesystem::path(preset_file_io::kPresetsDirectoryName) / "debug.json");
}

TEST_F(PresetDirectoryListTest, NoPresetsDirectoryReturnsEmptyNotAnException) {
    EXPECT_TRUE(preset_file_io::list_preset_files().empty());
}

TEST_F(PresetDirectoryListTest, ListsFilesPresentInThePresetsDirectory) {
    ASSERT_TRUE(preset_file_io::write_file(preset_file_io::preset_path("alpha.json"), "a").success);
    ASSERT_TRUE(preset_file_io::write_file(preset_file_io::preset_path("beta.json"), "b").success);

    std::vector<std::string> names = preset_file_io::list_preset_files();

    EXPECT_EQ(names, (std::vector<std::string>{"alpha.json", "beta.json"}));
}

TEST_F(PresetDirectoryListTest, IgnoresSubdirectoriesAndOnlyListsRegularFiles) {
    ASSERT_TRUE(preset_file_io::write_file(preset_file_io::preset_path("real.json"), "a").success);
    std::filesystem::create_directories(
        std::filesystem::path(preset_file_io::kPresetsDirectoryName) / "not_a_preset_dir");

    std::vector<std::string> names = preset_file_io::list_preset_files();

    EXPECT_EQ(names, (std::vector<std::string>{"real.json"}));
}
