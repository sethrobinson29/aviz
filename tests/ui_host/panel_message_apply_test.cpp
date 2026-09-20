#include "ui_host/panel_message_apply.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "compositor/compositor.h"
#include "compositor/compositor_math.h"
#include "compositor/preset_file_io.h"
#include "ui_host/fake_ui_host.h"

namespace {

nlohmann::json last_sent(const FakeUIHost& ui_host) {
    return nlohmann::json::parse(ui_host.sent_messages.back());
}

nlohmann::json sent_of_kind(const FakeUIHost& ui_host, const std::string& kind) {
    for (auto it = ui_host.sent_messages.rbegin(); it != ui_host.sent_messages.rend(); ++it) {
        nlohmann::json parsed = nlohmann::json::parse(*it);
        if (parsed.value("kind", "") == kind) {
            return parsed;
        }
    }
    ADD_FAILURE() << "no message of kind \"" << kind << "\" was sent";
    return nlohmann::json{};
}

}

TEST(PanelMessageApplyTest, RequestStatePushesLayerStackAndPresetList) {
    Compositor compositor;
    FakeUIHost ui_host;

    panel_message_apply::apply(PanelMessage{RequestStateMessage{}}, compositor, ui_host);

    ASSERT_EQ(ui_host.sent_messages.size(), 2u);
    EXPECT_EQ(sent_of_kind(ui_host, "layer_stack")["kind"], "layer_stack");
    EXPECT_EQ(sent_of_kind(ui_host, "preset_list")["kind"], "preset_list");
}

TEST(PanelMessageApplyTest, AddLayerAddsALayerAndPushesLayerStack) {
    Compositor compositor;
    FakeUIHost ui_host;

    panel_message_apply::apply(PanelMessage{AddLayerMessage{VisualType::Spectrum}}, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Spectrum);
    nlohmann::json state = last_sent(ui_host);
    EXPECT_EQ(state["kind"], "layer_stack");
    EXPECT_EQ(state["state"]["layers"].size(), 1u);
}

TEST(PanelMessageApplyTest, AddLayerAssignsANonWhiteDistinguishingTint) {
    Compositor compositor;
    FakeUIHost ui_host;

    panel_message_apply::apply(PanelMessage{AddLayerMessage{VisualType::Waveform}}, compositor, ui_host);
    panel_message_apply::apply(PanelMessage{AddLayerMessage{VisualType::Spectrum}}, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 2u);
    EXPECT_NE(compositor.layers()[0].tint_color.g, WHITE.g);
    EXPECT_NE(compositor.layers()[1].tint_color.g, WHITE.g);
    EXPECT_NE(compositor.layers()[0].tint_color.g, compositor.layers()[1].tint_color.g);
}

TEST(PanelMessageApplyTest, AddLayerAtCapSendsFailureStatusAndDoesNotGrow) {
    Compositor compositor;
    FakeUIHost ui_host;
    for (std::size_t i = 0; i < Compositor::kMaxLayers; ++i) {
        compositor.add_layer(VisualType::Waveform);
    }

    panel_message_apply::apply(PanelMessage{AddLayerMessage{VisualType::Waveform}}, compositor, ui_host);

    EXPECT_EQ(compositor.layers().size(), Compositor::kMaxLayers);
    nlohmann::json status = sent_of_kind(ui_host, "status");
    EXPECT_EQ(status["ok"], false);
}

TEST(PanelMessageApplyTest, RemoveLayerRemovesByLayerId) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);
    std::uint64_t target_id = compositor.layers()[0].layer_id;
    compositor.add_layer(VisualType::Spectrum);

    panel_message_apply::apply(PanelMessage{RemoveLayerMessage{target_id}}, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Spectrum);
}

TEST(PanelMessageApplyTest, RemoveLayerWithUnknownIdIsNoOpButStillPushesState) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);

    panel_message_apply::apply(PanelMessage{RemoveLayerMessage{999999}}, compositor, ui_host);

    EXPECT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(last_sent(ui_host)["kind"], "layer_stack");
}

TEST(PanelMessageApplyTest, RemoveLayerReassignsTintsOfRemainingLayersByNewIndex) {
    Compositor compositor;
    FakeUIHost ui_host;
    panel_message_apply::apply(PanelMessage{AddLayerMessage{VisualType::Waveform}}, compositor, ui_host);
    panel_message_apply::apply(PanelMessage{AddLayerMessage{VisualType::Spectrum}}, compositor, ui_host);
    panel_message_apply::apply(PanelMessage{AddLayerMessage{VisualType::Radial}}, compositor, ui_host);
    std::uint64_t first_id = compositor.layers()[0].layer_id;

    panel_message_apply::apply(PanelMessage{RemoveLayerMessage{first_id}}, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 2u);
    EXPECT_EQ(compositor.layers()[0].tint_color.g, compositor_math::layer_palette_color(0).g);
    EXPECT_EQ(compositor.layers()[1].tint_color.g, compositor_math::layer_palette_color(1).g);
}

TEST(PanelMessageApplyTest, DuplicateLayerAppendsACopy) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Spectrum);
    std::uint64_t source_id = compositor.layers()[0].layer_id;

    panel_message_apply::apply(PanelMessage{DuplicateLayerMessage{source_id}}, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 2u);
    EXPECT_EQ(compositor.layers()[1].visual_type, VisualType::Spectrum);
    EXPECT_NE(compositor.layers()[1].layer_id, source_id);
    EXPECT_EQ(compositor.layers()[1].tint_color.g, compositor_math::layer_palette_color(1).g);
}

TEST(PanelMessageApplyTest, DuplicateLayerAtCapSendsFailureStatus) {
    Compositor compositor;
    FakeUIHost ui_host;
    for (std::size_t i = 0; i < Compositor::kMaxLayers; ++i) {
        compositor.add_layer(VisualType::Waveform);
    }
    std::uint64_t source_id = compositor.layers()[0].layer_id;

    panel_message_apply::apply(PanelMessage{DuplicateLayerMessage{source_id}}, compositor, ui_host);

    EXPECT_EQ(compositor.layers().size(), Compositor::kMaxLayers);
    EXPECT_EQ(sent_of_kind(ui_host, "status")["ok"], false);
}

TEST(PanelMessageApplyTest, MoveLayerReordersByLayerId) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);
    compositor.add_layer(VisualType::Spectrum);
    std::uint64_t second_id = compositor.layers()[1].layer_id;

    panel_message_apply::apply(PanelMessage{MoveLayerMessage{second_id, 0}}, compositor, ui_host);

    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Spectrum);
    EXPECT_EQ(compositor.layers()[1].visual_type, VisualType::Waveform);
}

TEST(PanelMessageApplyTest, SetOpacityClampsOutOfRangeValue) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor.layers()[0].layer_id;

    panel_message_apply::apply(PanelMessage{SetLayerOpacityMessage{id, 5.0f}}, compositor, ui_host);

    EXPECT_FLOAT_EQ(compositor.layers()[0].opacity, 1.0f);
}

TEST(PanelMessageApplyTest, SetBlendModeUpdatesField) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor.layers()[0].layer_id;

    panel_message_apply::apply(PanelMessage{SetLayerBlendModeMessage{id, LayerBlendMode::Screen}}, compositor,
                                ui_host);

    EXPECT_EQ(compositor.layers()[0].blend_mode, LayerBlendMode::Screen);
}

TEST(PanelMessageApplyTest, SetFreqBandUpdatesFieldIncludingClearingToGlobal) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor.layers()[0].layer_id;

    panel_message_apply::apply(PanelMessage{SetLayerFreqBandMessage{id, std::optional<std::size_t>(4)}},
                                compositor, ui_host);
    EXPECT_EQ(compositor.layers()[0].freq_band, std::optional<std::size_t>(4));

    panel_message_apply::apply(PanelMessage{SetLayerFreqBandMessage{id, std::nullopt}}, compositor, ui_host);
    EXPECT_EQ(compositor.layers()[0].freq_band, std::nullopt);
}

TEST(PanelMessageApplyTest, SetReactivitySourceUpdatesField) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor.layers()[0].layer_id;

    panel_message_apply::apply(
        PanelMessage{SetLayerReactivitySourceMessage{id, ReactivitySource::BandAmplitude}}, compositor, ui_host);

    EXPECT_EQ(compositor.layers()[0].reactivity_source, ReactivitySource::BandAmplitude);
}

TEST(PanelMessageApplyTest, SetEnabledUpdatesField) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor.layers()[0].layer_id;

    panel_message_apply::apply(PanelMessage{SetLayerEnabledMessage{id, false}}, compositor, ui_host);

    EXPECT_FALSE(compositor.layers()[0].enabled);
}

TEST(PanelMessageApplyTest, SetSensitivityClampsOutOfRangeValue) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor.layers()[0].layer_id;

    panel_message_apply::apply(PanelMessage{SetLayerSensitivityMessage{id, 99.0f}}, compositor, ui_host);

    EXPECT_FLOAT_EQ(compositor.layers()[0].sensitivity, 4.0f);
}

TEST(PanelMessageApplyTest, UnknownLayerIdIsANoOpForEverySetter) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);

    panel_message_apply::apply(PanelMessage{SetLayerOpacityMessage{424242, 0.1f}}, compositor, ui_host);

    EXPECT_FLOAT_EQ(compositor.layers()[0].opacity, 1.0f);
}

class PanelMessageApplyPresetTest : public ::testing::Test {
protected:
    void SetUp() override { std::filesystem::remove_all(preset_file_io::kPresetsDirectoryName); }
    void TearDown() override { std::filesystem::remove_all(preset_file_io::kPresetsDirectoryName); }
};

TEST_F(PanelMessageApplyPresetTest, SavePresetWritesFileAndSendsSuccessStatus) {
    Compositor compositor;
    FakeUIHost ui_host;
    compositor.add_layer(VisualType::Waveform);

    panel_message_apply::apply(PanelMessage{SavePresetMessage{"apply-test.json"}}, compositor, ui_host);

    EXPECT_TRUE(std::filesystem::exists(preset_file_io::preset_path("apply-test.json")));
    nlohmann::json status = sent_of_kind(ui_host, "status");
    EXPECT_EQ(status["ok"], true);
    nlohmann::json preset_list = sent_of_kind(ui_host, "preset_list");
    EXPECT_NE(std::find(preset_list["files"].begin(), preset_list["files"].end(), "apply-test.json"),
              preset_list["files"].end());
}

TEST_F(PanelMessageApplyPresetTest, LoadPresetReplacesLayerStackAndSendsSuccessStatus) {
    Compositor writer;
    writer.add_layer(VisualType::Fractal);
    writer.add_layer(VisualType::Particles);
    FakeUIHost writer_host;
    panel_message_apply::apply(PanelMessage{SavePresetMessage{"apply-load-test.json"}}, writer, writer_host);

    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    FakeUIHost ui_host;

    panel_message_apply::apply(PanelMessage{LoadPresetMessage{"apply-load-test.json"}}, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 2u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Fractal);
    EXPECT_EQ(compositor.layers()[1].visual_type, VisualType::Particles);
    EXPECT_EQ(sent_of_kind(ui_host, "status")["ok"], true);
}

TEST_F(PanelMessageApplyPresetTest, LoadPresetOnMissingFileSendsFailureStatusAndLeavesStackUnchanged) {
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    FakeUIHost ui_host;

    panel_message_apply::apply(PanelMessage{LoadPresetMessage{"does-not-exist.json"}}, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Waveform);
    EXPECT_EQ(sent_of_kind(ui_host, "status")["ok"], false);
}

TEST_F(PanelMessageApplyPresetTest, LoadPresetOnMalformedFileSendsFailureStatusAndLeavesStackUnchanged) {
    preset_file_io::write_file(preset_file_io::preset_path("malformed.json"), "{ not valid json");
    Compositor compositor;
    compositor.add_layer(VisualType::Waveform);
    FakeUIHost ui_host;

    panel_message_apply::apply(PanelMessage{LoadPresetMessage{"malformed.json"}}, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(sent_of_kind(ui_host, "status")["ok"], false);
}
