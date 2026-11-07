#include "compositor/preset_file_io.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "compositor/layer.h"
#include "compositor/layer_preset.h"
#include "compositor/palette_test_fixtures.h"
#include "visuals/spectrum/spectrum_visual.h"
#include "visuals/waveform/waveform_visual.h"

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

TEST_F(PresetFileIoTest, PaletteOverridesRoundTripStopForStopThroughFileOnDisk) {
    const std::vector<std::optional<Palette>> palettes = palette_fixtures::all_cases();
    std::vector<Layer> layers;
    std::uint64_t next_id = 1;
    for (const std::optional<Palette>& palette : palettes) {
        Layer waveform{VisualType::Waveform, std::make_unique<WaveformVisual>()};
        waveform.layer_id = next_id++;
        waveform.palette_override = palette;
        layers.push_back(std::move(waveform));
        Layer spectrum{VisualType::Spectrum, std::make_unique<SpectrumVisual>()};
        spectrum.layer_id = next_id++;
        spectrum.palette_override = palette;
        layers.push_back(std::move(spectrum));
    }
    const std::string saved_text = layer_preset::serialize(layers);
    const std::string path = (scratch_dir_ / "palettes.json").string();
    ASSERT_TRUE(preset_file_io::write_file(path, saved_text).success);

    preset_file_io::ReadResult read_result = preset_file_io::read_file(path);
    ASSERT_TRUE(read_result.success) << read_result.error;
    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(read_result.contents);

    ASSERT_TRUE(loaded.has_value());
    ASSERT_EQ(loaded->size(), layers.size());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        SCOPED_TRACE(i);
        palette_fixtures::expect_same_stops((*loaded)[i].palette_override, layers[i].palette_override);
    }
    EXPECT_EQ(layer_preset::serialize(*loaded), saved_text);
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
    const std::string joined = preset_file_io::preset_path("C:\\apps\\aviz\\presets", "debug.json");
    EXPECT_EQ(std::filesystem::path(joined), std::filesystem::path("C:\\apps\\aviz\\presets") / "debug.json");
}

TEST(PresetsDirectoryTest, IsThePresetsFolderInsideTheApplicationDirectory) {
    EXPECT_EQ(std::filesystem::path(preset_file_io::presets_directory_in("C:\\apps\\aviz\\")),
              std::filesystem::path("C:\\apps\\aviz\\presets"));
    EXPECT_EQ(std::filesystem::path(preset_file_io::presets_directory_in("C:\\apps\\aviz")),
              std::filesystem::path("C:\\apps\\aviz\\presets"));
}

TEST(PresetsDirectoryTest, DoesNotDependOnTheWorkingDirectory) {
    const std::filesystem::path original_working_dir = std::filesystem::current_path();
    const std::string before = preset_file_io::presets_directory_in("C:\\apps\\aviz\\");

    std::filesystem::current_path(std::filesystem::temp_directory_path());
    const std::string after = preset_file_io::presets_directory_in("C:\\apps\\aviz\\");
    std::filesystem::current_path(original_working_dir);

    EXPECT_EQ(before, after);
    EXPECT_TRUE(std::filesystem::path(after).is_absolute());
}

TEST(PresetsDirectoryTest, RelativeApplicationDirectoryIsMadeAbsolute) {
    EXPECT_TRUE(std::filesystem::path(preset_file_io::presets_directory_in("./")).is_absolute());
}

TEST_F(PresetFileIoTest, ReadOfAFileExactlyAtTheCapSucceeds) {
    const std::string path = (scratch_dir_ / "at_cap.json").string();
    ASSERT_TRUE(preset_file_io::write_file(path, std::string(16, 'x')).success);

    preset_file_io::ReadResult result = preset_file_io::read_file(path, 16);

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.too_large);
    EXPECT_EQ(result.contents, std::string(16, 'x'));
}

TEST_F(PresetFileIoTest, ReadOfAFileOneByteOverTheCapFailsAsTooLargeWithNoContents) {
    const std::string path = (scratch_dir_ / "over_cap.json").string();
    ASSERT_TRUE(preset_file_io::write_file(path, std::string(17, 'x')).success);

    preset_file_io::ReadResult result = preset_file_io::read_file(path, 16);

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.too_large);
    EXPECT_TRUE(result.contents.empty());
    EXPECT_FALSE(result.error.empty());
}

TEST_F(PresetFileIoTest, DefaultCapAcceptsOneMebibyteAndRejectsOneByteMore) {
    const std::string at_cap = (scratch_dir_ / "default_at_cap.json").string();
    const std::string over_cap = (scratch_dir_ / "default_over_cap.json").string();
    ASSERT_TRUE(preset_file_io::write_file(at_cap, std::string(preset_file_io::kMaxPresetFileBytes, 'y')).success);
    ASSERT_TRUE(
        preset_file_io::write_file(over_cap, std::string(preset_file_io::kMaxPresetFileBytes + 1, 'y')).success);

    EXPECT_EQ(preset_file_io::kMaxPresetFileBytes, 1024u * 1024u);
    EXPECT_TRUE(preset_file_io::read_file(at_cap).success);
    EXPECT_TRUE(preset_file_io::read_file(over_cap).too_large);
}

TEST_F(PresetFileIoTest, EmptyFileReadsAsSuccessWithEmptyContents) {
    const std::string path = (scratch_dir_ / "empty.json").string();
    ASSERT_TRUE(preset_file_io::write_file(path, "").success);

    preset_file_io::ReadResult result = preset_file_io::read_file(path);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.contents.empty());
}

TEST_F(PresetFileIoTest, MissingFileIsNotReportedAsTooLarge) {
    EXPECT_FALSE(preset_file_io::read_file((scratch_dir_ / "nope.json").string()).too_large);
}

TEST_F(PresetFileIoTest, NonAsciiPresetFilenameRoundTripsToTheRealFilenameOnDisk) {
    const std::string dir = scratch_dir_.u8string();
    const std::string path = preset_file_io::preset_path(dir, "\xE6\x97\xA5\xE6\x9C\xAC caf\xC3\xA9.json");

    ASSERT_TRUE(preset_file_io::write_file(path, "contents").success);

    EXPECT_TRUE(std::filesystem::exists(scratch_dir_ / L"\x65E5\x672C caf\x00E9.json"));
    preset_file_io::ReadResult read_back = preset_file_io::read_file(path);
    ASSERT_TRUE(read_back.success);
    EXPECT_EQ(read_back.contents, "contents");
}

TEST(PresetsDirectoryTest, NonAsciiApplicationDirectoryComesBackAsUtf8) {
    const std::filesystem::path app_dir = std::filesystem::temp_directory_path() / L"caf\x00E9";

    const std::string presets_dir = preset_file_io::presets_directory_in(app_dir.string());

    EXPECT_EQ(std::filesystem::u8path(presets_dir), (app_dir / "presets").lexically_normal());
}

TEST(IsSafePresetFilenameTest, RejectsDriveQualifiedAndAlternateDataStreamNames) {
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("C:evil.json"));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("D:x.json"));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("preset.json:hidden.json"));
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("name:stream.json"));
}

TEST(PresetPathTest, DriveQualifiedFilenameEscapesPresetsDirectorySoValidationMustRejectIt) {
    const std::filesystem::path presets_dir("C:\\apps\\aviz\\presets");
    const std::filesystem::path joined(preset_file_io::preset_path(presets_dir.string(), "D:evil.json"));
    EXPECT_NE(joined.parent_path(), presets_dir);
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("D:evil.json"));
}

TEST(IsSafePresetFilenameTest, RejectsCharactersWindowsDoesNotAllowInFilenames) {
    for (const char* name : {"a<b.json", "a>b.json", "a\"b.json", "a|b.json", "a?b.json", "a*b.json"}) {
        EXPECT_FALSE(preset_file_io::is_safe_preset_filename(name)) << name;
    }
    EXPECT_FALSE(preset_file_io::is_safe_preset_filename("bad\x7Fname.json"));
}

TEST(IsSafePresetFilenameTest, RejectsWindowsReservedDeviceNamesInAnyCase) {
    for (const char* name : {"NUL.json", "nul.json", "CON.json", "con.json", "PRN.json", "AUX.json", "COM1.json",
                             "com9.json", "LPT1.json", "lpt9.json", "nul.tar.json", "aux .json"}) {
        EXPECT_FALSE(preset_file_io::is_safe_preset_filename(name)) << name;
    }
}

TEST(IsSafePresetFilenameTest, AcceptsOrdinaryNamesThatOnlyResembleReservedOnes) {
    for (const char* name : {"console.json", "nulls.json", "com10.json", "lpt.json", "my preset (v2).json",
                             "auxiliary-mix.json", "a.b.c.json"}) {
        EXPECT_TRUE(preset_file_io::is_safe_preset_filename(name)) << name;
    }
}
