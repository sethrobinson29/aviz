#include "ui_host/panel_message.h"

#include <optional>
#include <variant>

#include <gtest/gtest.h>

namespace {

template <typename T>
T expect_type(const std::optional<PanelMessage>& message) {
    if (!message.has_value() || !std::holds_alternative<T>(*message)) {
        ADD_FAILURE() << "message did not parse to the expected type";
        return T{};
    }
    return std::get<T>(*message);
}

}

TEST(PanelMessageParseTest, RejectsEmptyString) {
    EXPECT_FALSE(panel_message::parse("").has_value());
}

TEST(PanelMessageParseTest, RejectsMalformedJson) {
    EXPECT_FALSE(panel_message::parse("{not valid json").has_value());
}

TEST(PanelMessageParseTest, RejectsNonObjectJson) {
    EXPECT_FALSE(panel_message::parse("42").has_value());
    EXPECT_FALSE(panel_message::parse("[1,2,3]").has_value());
    EXPECT_FALSE(panel_message::parse(R"("add_layer")").has_value());
}

TEST(PanelMessageParseTest, RejectsMissingTypeField) {
    EXPECT_FALSE(panel_message::parse(R"({"foo":"bar"})").has_value());
}

TEST(PanelMessageParseTest, RejectsUnknownTypeValue) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"delete_everything"})").has_value());
}

TEST(PanelMessageParseTest, RejectsWrongTypeFieldType) {
    EXPECT_FALSE(panel_message::parse(R"({"type":123})").has_value());
    EXPECT_FALSE(panel_message::parse(R"({"type":null})").has_value());
}

TEST(PanelMessageParseTest, ParsesRequestState) {
    auto message = panel_message::parse(R"({"type":"request_state"})");
    expect_type<RequestStateMessage>(message);
}

TEST(PanelMessageParseTest, ParsesAddLayer) {
    auto message = panel_message::parse(R"({"type":"add_layer","visual_type":"Fractal"})");
    AddLayerMessage typed = expect_type<AddLayerMessage>(message);
    EXPECT_EQ(typed.visual_type, VisualType::Fractal);
}

TEST(PanelMessageParseTest, AddLayerRejectsUnknownVisualType) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"add_layer","visual_type":"Bogus"})").has_value());
}

TEST(PanelMessageParseTest, AddLayerRejectsMissingVisualType) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"add_layer"})").has_value());
}

TEST(PanelMessageParseTest, ParsesRemoveLayer) {
    auto message = panel_message::parse(R"({"type":"remove_layer","layer_id":42})");
    RemoveLayerMessage typed = expect_type<RemoveLayerMessage>(message);
    EXPECT_EQ(typed.layer_id, 42u);
}

TEST(PanelMessageParseTest, RemoveLayerRejectsNegativeLayerId) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"remove_layer","layer_id":-1})").has_value());
}

TEST(PanelMessageParseTest, RemoveLayerRejectsFloatLayerId) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"remove_layer","layer_id":1.5})").has_value());
}

TEST(PanelMessageParseTest, RemoveLayerRejectsStringLayerId) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"remove_layer","layer_id":"7"})").has_value());
}

TEST(PanelMessageParseTest, RemoveLayerRejectsMissingLayerId) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"remove_layer"})").has_value());
}

TEST(PanelMessageParseTest, ParsesDuplicateLayer) {
    auto message = panel_message::parse(R"({"type":"duplicate_layer","layer_id":9})");
    DuplicateLayerMessage typed = expect_type<DuplicateLayerMessage>(message);
    EXPECT_EQ(typed.layer_id, 9u);
}

TEST(PanelMessageParseTest, ParsesMoveLayer) {
    auto message = panel_message::parse(R"({"type":"move_layer","layer_id":3,"to_index":2})");
    MoveLayerMessage typed = expect_type<MoveLayerMessage>(message);
    EXPECT_EQ(typed.layer_id, 3u);
    EXPECT_EQ(typed.to_index, 2u);
}

TEST(PanelMessageParseTest, MoveLayerRejectsNegativeToIndex) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"move_layer","layer_id":3,"to_index":-1})").has_value());
}

TEST(PanelMessageParseTest, MoveLayerRejectsMissingToIndex) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"move_layer","layer_id":3})").has_value());
}

TEST(PanelMessageParseTest, ParsesSetOpacity) {
    auto message = panel_message::parse(R"({"type":"set_opacity","layer_id":1,"opacity":0.5})");
    SetLayerOpacityMessage typed = expect_type<SetLayerOpacityMessage>(message);
    EXPECT_EQ(typed.layer_id, 1u);
    EXPECT_FLOAT_EQ(typed.opacity, 0.5f);
}

TEST(PanelMessageParseTest, SetOpacityAcceptsIntegerLiteral) {
    auto message = panel_message::parse(R"({"type":"set_opacity","layer_id":1,"opacity":1})");
    SetLayerOpacityMessage typed = expect_type<SetLayerOpacityMessage>(message);
    EXPECT_FLOAT_EQ(typed.opacity, 1.0f);
}

TEST(PanelMessageParseTest, SetOpacityOutOfRangeParsesAndIsClampedLaterByCompositor) {
    auto message = panel_message::parse(R"({"type":"set_opacity","layer_id":1,"opacity":5.0})");
    SetLayerOpacityMessage typed = expect_type<SetLayerOpacityMessage>(message);
    EXPECT_FLOAT_EQ(typed.opacity, 5.0f);
}

TEST(PanelMessageParseTest, SetOpacityRejectsStringValue) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_opacity","layer_id":1,"opacity":"0.5"})").has_value());
}

TEST(PanelMessageParseTest, ParsesSetBlendMode) {
    auto message = panel_message::parse(R"({"type":"set_blend_mode","layer_id":1,"blend_mode":"Additive"})");
    SetLayerBlendModeMessage typed = expect_type<SetLayerBlendModeMessage>(message);
    EXPECT_EQ(typed.blend_mode, LayerBlendMode::Additive);
}

TEST(PanelMessageParseTest, SetBlendModeRejectsUnknownValue) {
    EXPECT_FALSE(
        panel_message::parse(R"({"type":"set_blend_mode","layer_id":1,"blend_mode":"Overlay"})").has_value());
}

TEST(PanelMessageParseTest, ParsesSetFreqBandWithNumber) {
    auto message = panel_message::parse(R"({"type":"set_freq_band","layer_id":1,"freq_band":5})");
    SetLayerFreqBandMessage typed = expect_type<SetLayerFreqBandMessage>(message);
    EXPECT_EQ(typed.freq_band, std::optional<std::size_t>(5));
}

TEST(PanelMessageParseTest, ParsesSetFreqBandWithNull) {
    auto message = panel_message::parse(R"({"type":"set_freq_band","layer_id":1,"freq_band":null})");
    SetLayerFreqBandMessage typed = expect_type<SetLayerFreqBandMessage>(message);
    EXPECT_EQ(typed.freq_band, std::nullopt);
}

TEST(PanelMessageParseTest, SetFreqBandRejectsAtOrAboveNumBands) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_freq_band","layer_id":1,"freq_band":16})").has_value());
}

TEST(PanelMessageParseTest, SetFreqBandRejectsNegative) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_freq_band","layer_id":1,"freq_band":-1})").has_value());
}

TEST(PanelMessageParseTest, SetFreqBandRejectsMissingField) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_freq_band","layer_id":1})").has_value());
}

TEST(PanelMessageParseTest, ParsesSetReactivitySource) {
    auto message =
        panel_message::parse(R"({"type":"set_reactivity_source","layer_id":1,"reactivity_source":"BandOnset"})");
    SetLayerReactivitySourceMessage typed = expect_type<SetLayerReactivitySourceMessage>(message);
    EXPECT_EQ(typed.reactivity_source, ReactivitySource::BandOnset);
}

TEST(PanelMessageParseTest, SetReactivitySourceRejectsUnknownValue) {
    EXPECT_FALSE(panel_message::parse(
                     R"({"type":"set_reactivity_source","layer_id":1,"reactivity_source":"Bogus"})")
                     .has_value());
}

TEST(PanelMessageParseTest, ParsesSetEnabled) {
    auto message = panel_message::parse(R"({"type":"set_enabled","layer_id":1,"enabled":false})");
    SetLayerEnabledMessage typed = expect_type<SetLayerEnabledMessage>(message);
    EXPECT_FALSE(typed.enabled);
}

TEST(PanelMessageParseTest, SetEnabledRejectsNonBooleanValue) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_enabled","layer_id":1,"enabled":1})").has_value());
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_enabled","layer_id":1,"enabled":"true"})").has_value());
}

TEST(PanelMessageParseTest, ParsesSetSensitivity) {
    auto message = panel_message::parse(R"({"type":"set_sensitivity","layer_id":1,"sensitivity":2.5})");
    SetLayerSensitivityMessage typed = expect_type<SetLayerSensitivityMessage>(message);
    EXPECT_FLOAT_EQ(typed.sensitivity, 2.5f);
}

TEST(PanelMessageParseTest, ParsesLoadPreset) {
    auto message = panel_message::parse(R"({"type":"load_preset","filename":"my-preset.json"})");
    LoadPresetMessage typed = expect_type<LoadPresetMessage>(message);
    EXPECT_EQ(typed.filename, "my-preset.json");
}

TEST(PanelMessageParseTest, LoadPresetRejectsPathTraversal) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"load_preset","filename":"../secret.json"})").has_value());
}

TEST(PanelMessageParseTest, LoadPresetRejectsPathSeparators) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"load_preset","filename":"sub/dir.json"})").has_value());
    EXPECT_FALSE(panel_message::parse(R"({"type":"load_preset","filename":"sub\\dir.json"})").has_value());
}

TEST(PanelMessageParseTest, LoadPresetRejectsNonJsonExtension) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"load_preset","filename":"preset.txt"})").has_value());
}

TEST(PanelMessageParseTest, LoadPresetRejectsEmptyFilename) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"load_preset","filename":""})").has_value());
}

TEST(PanelMessageParseTest, ParsesSavePreset) {
    auto message = panel_message::parse(R"({"type":"save_preset","filename":"new-preset.json"})");
    SavePresetMessage typed = expect_type<SavePresetMessage>(message);
    EXPECT_EQ(typed.filename, "new-preset.json");
}

TEST(PanelMessageParseTest, SavePresetRejectsPathTraversal) {
    EXPECT_FALSE(
        panel_message::parse(R"({"type":"save_preset","filename":"../../etc/passwd.json"})").has_value());
}

TEST(PanelMessageParseTest, IgnoresUnknownExtraFields) {
    auto message =
        panel_message::parse(R"({"type":"add_layer","visual_type":"Waveform","extra":123,"nested":{"a":1}})");
    expect_type<AddLayerMessage>(message);
}

TEST(PanelMessageParseTest, ParsesSetDockedTrue) {
    auto message = panel_message::parse(R"({"type":"set_docked","docked":true})");
    SetDockedMessage typed = expect_type<SetDockedMessage>(message);
    EXPECT_TRUE(typed.docked);
}

TEST(PanelMessageParseTest, ParsesSetDockedFalse) {
    auto message = panel_message::parse(R"({"type":"set_docked","docked":false})");
    SetDockedMessage typed = expect_type<SetDockedMessage>(message);
    EXPECT_FALSE(typed.docked);
}

TEST(PanelMessageParseTest, SetDockedRejectsNonBooleanValue) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_docked","docked":1})").has_value());
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_docked","docked":"true"})").has_value());
}

TEST(PanelMessageParseTest, SetDockedRejectsMissingField) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_docked"})").has_value());
}

TEST(PanelMessageParseTest, ParsesSetFullscreenTrue) {
    auto message = panel_message::parse(R"({"type":"set_fullscreen","fullscreen":true})");
    SetFullscreenMessage typed = expect_type<SetFullscreenMessage>(message);
    EXPECT_TRUE(typed.fullscreen);
}

TEST(PanelMessageParseTest, SetFullscreenRejectsNonBooleanValue) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_fullscreen","fullscreen":"yes"})").has_value());
}

TEST(PanelMessageParseTest, SetFullscreenRejectsMissingField) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_fullscreen"})").has_value());
}

TEST(PanelMessageParseTest, ParsesSwitchSource) {
    auto message = panel_message::parse(R"({"type":"switch_source"})");
    expect_type<SwitchSourceMessage>(message);
}

TEST(PanelMessageParseTest, SwitchSourceIgnoresExtraFields) {
    auto message = panel_message::parse(R"({"type":"switch_source","unexpected":true})");
    expect_type<SwitchSourceMessage>(message);
}
