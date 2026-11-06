#include "ui_host/panel_message.h"

#include <limits>
#include <optional>
#include <variant>

#include <gtest/gtest.h>

#include "compositor/palette_test_fixtures.h"

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

TEST(PanelMessageParseTest, ParsesSetPaletteOverrideWithStops) {
    auto message = panel_message::parse(
        R"({"type":"set_palette_override","layer_id":7,"palette_override":{"stops":[)"
        R"({"position":0,"color":{"r":255,"g":220,"b":0,"a":255}},)"
        R"({"position":0.5,"color":{"r":255,"g":80,"b":0,"a":255}},)"
        R"({"position":1,"color":{"r":120,"g":0,"b":200,"a":255}}]}})");

    auto parsed = expect_type<SetPaletteOverrideMessage>(message);

    EXPECT_EQ(parsed.layer_id, 7u);
    ASSERT_NE(parsed.palette_override, nullptr);
    palette_fixtures::expect_same_stops(*parsed.palette_override, palette_fixtures::sunset());
}

TEST(PanelMessageParseTest, ParsesSetPaletteOverrideWithNullAsClear) {
    auto message = panel_message::parse(R"({"type":"set_palette_override","layer_id":7,"palette_override":null})");

    auto parsed = expect_type<SetPaletteOverrideMessage>(message);

    EXPECT_EQ(parsed.layer_id, 7u);
    EXPECT_EQ(parsed.palette_override, nullptr);
}

TEST(PanelMessageParseTest, SetPaletteOverrideClampsStopPositionsLikePresetLoad) {
    auto message = panel_message::parse(
        R"({"type":"set_palette_override","layer_id":1,"palette_override":{"stops":[)"
        R"({"position":-5,"color":{"r":1,"g":2,"b":3,"a":4}},)"
        R"({"position":1e300,"color":{"r":5,"g":6,"b":7,"a":8}}]}})");

    auto parsed = expect_type<SetPaletteOverrideMessage>(message);

    ASSERT_NE(parsed.palette_override, nullptr);
    EXPECT_FLOAT_EQ(parsed.palette_override->stops[0].position, 0.0f);
    EXPECT_FLOAT_EQ(parsed.palette_override->stops[1].position, 1.0f);
}

TEST(PanelMessageParseTest, SetPaletteOverrideRejectsMissingField) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_palette_override","layer_id":1})").has_value());
}

TEST(PanelMessageParseTest, SetPaletteOverrideRejectsMissingOrInvalidLayerId) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_palette_override","palette_override":null})").has_value());
    EXPECT_FALSE(
        panel_message::parse(R"({"type":"set_palette_override","layer_id":-1,"palette_override":null})").has_value());
}

TEST(PanelMessageParseTest, SetPaletteOverrideRejectsNonObjectAndMissingStops) {
    for (const char* value : {R"("Sunset")", "1", "true", "[]", "{}", R"({"stops":"nope"})"}) {
        const std::string text =
            std::string(R"({"type":"set_palette_override","layer_id":1,"palette_override":)") + value + "}";
        EXPECT_FALSE(panel_message::parse(text).has_value()) << value;
    }
}

TEST(PanelMessageParseTest, SetPaletteOverrideRejectsEmptyStops) {
    EXPECT_FALSE(
        panel_message::parse(R"({"type":"set_palette_override","layer_id":1,"palette_override":{"stops":[]}})")
            .has_value());
}

TEST(PanelMessageParseTest, SetPaletteOverrideRejectsDescendingStops) {
    EXPECT_FALSE(panel_message::parse(
                     R"({"type":"set_palette_override","layer_id":1,"palette_override":{"stops":[)"
                     R"({"position":0.8,"color":{"r":1,"g":2,"b":3,"a":4}},)"
                     R"({"position":0.2,"color":{"r":5,"g":6,"b":7,"a":8}}]}})")
                     .has_value());
}

TEST(PanelMessageParseTest, SetPaletteOverrideRejectsMalformedStops) {
    for (const char* stop : {"1", R"({"position":0})", R"({"color":{"r":1,"g":2,"b":3,"a":4}})",
                             R"({"position":"0","color":{"r":1,"g":2,"b":3,"a":4}})",
                             R"({"position":0,"color":"red"})", R"({"position":0,"color":{"r":1,"g":2,"b":3}})",
                             R"({"position":0,"color":{"r":256,"g":2,"b":3,"a":4}})",
                             R"({"position":0,"color":{"r":-1,"g":2,"b":3,"a":4}})"}) {
        const std::string text =
            std::string(R"({"type":"set_palette_override","layer_id":1,"palette_override":{"stops":[)") + stop +
            "]}}";
        EXPECT_FALSE(panel_message::parse(text).has_value()) << stop;
    }
}

TEST(PanelMessageParseTest, ParsesSetVisualParam) {
    auto message =
        panel_message::parse(R"({"type":"set_visual_param","layer_id":1,"param_name":"spawn_rate_per_frame","value":5.0})");
    SetVisualParamMessage typed = expect_type<SetVisualParamMessage>(message);
    EXPECT_EQ(typed.layer_id, 1u);
    EXPECT_EQ(typed.param_name, "spawn_rate_per_frame");
    ASSERT_TRUE(std::holds_alternative<float>(typed.value));
    EXPECT_FLOAT_EQ(std::get<float>(typed.value), 5.0f);
}

TEST(PanelMessageParseTest, SetVisualParamParsesJsonBooleanAsBoolNotFloat) {
    auto message = panel_message::parse(
        R"({"type":"set_visual_param","layer_id":1,"param_name":"ripple_on_onset_enabled","value":false})");
    SetVisualParamMessage typed = expect_type<SetVisualParamMessage>(message);
    ASSERT_TRUE(std::holds_alternative<bool>(typed.value));
    EXPECT_FALSE(std::get<bool>(typed.value));
}

TEST(PanelMessageParseTest, SetVisualParamParsesJsonNumberOneAsFloatNotBool) {
    auto message = panel_message::parse(
        R"({"type":"set_visual_param","layer_id":1,"param_name":"ripple_on_onset_enabled","value":1})");
    SetVisualParamMessage typed = expect_type<SetVisualParamMessage>(message);
    EXPECT_TRUE(std::holds_alternative<float>(typed.value));
}

TEST(PanelMessageParseTest, SetVisualParamRejectsStringNullArrayAndObjectValues) {
    EXPECT_FALSE(panel_message::parse(
                     R"({"type":"set_visual_param","layer_id":1,"param_name":"x","value":"true"})")
                     .has_value());
    EXPECT_FALSE(panel_message::parse(
                     R"({"type":"set_visual_param","layer_id":1,"param_name":"x","value":null})")
                     .has_value());
    EXPECT_FALSE(panel_message::parse(
                     R"({"type":"set_visual_param","layer_id":1,"param_name":"x","value":[1]})")
                     .has_value());
    EXPECT_FALSE(panel_message::parse(
                     R"({"type":"set_visual_param","layer_id":1,"param_name":"x","value":{"v":1}})")
                     .has_value());
}

TEST(PanelMessageParseTest, SetVisualParamRejectsEmptyParamName) {
    EXPECT_FALSE(
        panel_message::parse(R"({"type":"set_visual_param","layer_id":1,"param_name":"","value":1.0})")
            .has_value());
}

TEST(PanelMessageParseTest, SetVisualParamRejectsParamNameWithUnexpectedCharacters) {
    EXPECT_FALSE(panel_message::parse(
                     R"({"type":"set_visual_param","layer_id":1,"param_name":"foo.bar","value":1.0})")
                     .has_value());
    EXPECT_FALSE(panel_message::parse(
                     R"({"type":"set_visual_param","layer_id":1,"param_name":"foo bar","value":1.0})")
                     .has_value());
}

TEST(PanelMessageParseTest, SetVisualParamRejectsMissingValue) {
    EXPECT_FALSE(panel_message::parse(
                     R"({"type":"set_visual_param","layer_id":1,"param_name":"spawn_rate_per_frame"})")
                     .has_value());
}

TEST(PanelMessageParseTest, SetVisualParamRejectsNonStringParamName) {
    EXPECT_FALSE(
        panel_message::parse(R"({"type":"set_visual_param","layer_id":1,"param_name":5,"value":1.0})")
            .has_value());
}

TEST(PanelMessageParseTest, PresetLoadingIsNotAcceptedFromThePanelMessageChannel) {
    for (const char* text : {
             R"({"type":"load_preset","filename":"my-preset.json"})",
             R"({"type":"load_preset","filename":"../secret.json"})",
             R"({"type":"load_preset_file","path":"C:\\Users\\someone\\preset.json"})",
             R"({"type":"load_preset_file","path":"preset.json"})",
             R"({"type":"load_preset_file"})",
             R"({"type":"open_preset_dialog"})",
             R"({"type":"open_preset_dialog","path":"C:\\Windows\\win.ini"})",
             R"({"type":"open_file_dialog"})",
         }) {
        EXPECT_FALSE(panel_message::parse(text).has_value()) << text;
    }
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

TEST(PanelMessageParseTest, ImportFileIsNotAcceptedFromThePanelMessageChannel) {
    for (const char* text : {
             R"({"type":"import_file","path":"C:\\music\\track.wav"})",
             R"({"type":"import_file","path":"track.wav"})",
             R"({"type":"import_file","path":"track.mp3"})",
             R"({"type":"import_file","path":"track.flac"})",
             R"({"type":"import_file","path":"C:\\Users\\someone\\private.wav"})",
             R"({"type":"import_file","path":"track.exe"})",
             R"({"type":"import_file","path":""})",
             R"({"type":"import_file","path":123})",
             R"({"type":"import_file"})",
         }) {
        EXPECT_FALSE(panel_message::parse(text).has_value()) << text;
    }
}

TEST(PanelMessageParseTest, ParsesEnableLoopback) {
    auto message = panel_message::parse(R"({"type":"enable_loopback"})");
    expect_type<EnableLoopbackMessage>(message);
}

TEST(PanelMessageParseTest, OpacityBeyondFloatRangeParsesToAFiniteFloat) {
    auto high = panel_message::parse(R"({"type":"set_opacity","layer_id":1,"opacity":1e300})");
    EXPECT_EQ(expect_type<SetLayerOpacityMessage>(high).opacity, std::numeric_limits<float>::max());

    auto low = panel_message::parse(R"({"type":"set_opacity","layer_id":1,"opacity":-1e300})");
    EXPECT_EQ(expect_type<SetLayerOpacityMessage>(low).opacity, std::numeric_limits<float>::lowest());
}

TEST(PanelMessageParseTest, SensitivityBeyondFloatRangeParsesToAFiniteFloat) {
    auto high = panel_message::parse(R"({"type":"set_sensitivity","layer_id":1,"sensitivity":1e300})");
    EXPECT_EQ(expect_type<SetLayerSensitivityMessage>(high).sensitivity, std::numeric_limits<float>::max());
}

TEST(PanelMessageParseTest, VisualParamValueBeyondFloatRangeParsesToAFiniteFloat) {
    auto high = panel_message::parse(
        R"({"type":"set_visual_param","layer_id":1,"param_name":"spawn_rate_per_frame","value":1e300})");
    SetVisualParamMessage typed = expect_type<SetVisualParamMessage>(high);
    ASSERT_TRUE(std::holds_alternative<float>(typed.value));
    EXPECT_EQ(std::get<float>(typed.value), std::numeric_limits<float>::max());

    auto low = panel_message::parse(
        R"({"type":"set_visual_param","layer_id":1,"param_name":"spawn_rate_per_frame","value":-1e300})");
    EXPECT_EQ(std::get<float>(expect_type<SetVisualParamMessage>(low).value),
              std::numeric_limits<float>::lowest());
}

TEST(PanelMessageParseTest, RejectsPresetFilenamesThatWouldEscapeThePresetsDirectoryOnWindows) {
    for (const char* type : {"save_preset"}) {
        for (const char* filename : {"C:evil.json", "name:stream.json", "NUL.json", "a*b.json"}) {
            const std::string text =
                std::string(R"({"type":")") + type + R"(","filename":")" + filename + R"("})";
            EXPECT_FALSE(panel_message::parse(text).has_value()) << text;
        }
    }
}

TEST(PanelMessageParseTest, ParsesSetLayerName) {
    auto message = expect_type<SetLayerNameMessage>(
        panel_message::parse(R"({"type":"set_layer_name","layer_id":7,"name":"Kick pulse"})"));

    EXPECT_EQ(message.layer_id, 7u);
    EXPECT_EQ(message.name, "Kick pulse");
}

TEST(PanelMessageParseTest, SetLayerNameRejectsWrongJsonTypes) {
    for (const char* name : {"null", "5", "true", "[]", "{}"}) {
        const std::string text = std::string(R"({"type":"set_layer_name","layer_id":7,"name":)") + name + "}";
        EXPECT_FALSE(panel_message::parse(text).has_value()) << name;
    }
}

TEST(PanelMessageParseTest, SetLayerNameRejectsMissingFields) {
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_layer_name","layer_id":7})").has_value());
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_layer_name","name":"x"})").has_value());
    EXPECT_FALSE(panel_message::parse(R"({"type":"set_layer_name","layer_id":-1,"name":"x"})").has_value());
}

TEST(PanelMessageParseTest, SetLayerNameLeavesContentChecksToApply) {
    auto message = expect_type<SetLayerNameMessage>(panel_message::parse(
        std::string(R"({"type":"set_layer_name","layer_id":7,"name":")") + std::string(51, 'a') + "\"}"));

    EXPECT_EQ(message.name.size(), 51u);
}

TEST(PanelMessageParseTest, SetLayerNameRejectsRawInvalidUtf8AndLoneSurrogates) {
    const std::string prefix = R"({"type":"set_layer_name","layer_id":7,"name":")";
    const char backslash = static_cast<char>(92);

    EXPECT_FALSE(panel_message::parse(prefix + "caf\xE9\"}").has_value());
    EXPECT_FALSE(panel_message::parse(prefix + backslash + "ud800\"}").has_value());
}
