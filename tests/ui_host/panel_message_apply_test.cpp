#include "ui_host/panel_message_apply.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "app/audio_source_controller.h"
#include "app/window_layout_controller.h"
#include "audio/audio_config.h"
#include "audio/mock_producer.h"
#include "audio/producer_host.h"
#include "audio/ring_buffer.h"
#include "compositor/compositor.h"
#include "compositor/compositor_math.h"
#include "compositor/layer_preset.h"
#include "compositor/palette_test_fixtures.h"
#include "compositor/preset_file_io.h"
#include "ui_host/fake_ui_host.h"
#include "ui_host/panel_apply_test_fixture.h"
#include "ui_host/panel_dispatch_queue.h"
#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/fractal/fractal_visual.h"
#include "visuals/lissajous/lissajous_visual.h"
#include "visuals/particles/particles_visual.h"
#include "visuals/radial/radial_visual.h"
#include "visuals/visual_config.h"

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

nlohmann::json color_wire_json(const Color& c) {
    return nlohmann::json{{"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a}};
}

nlohmann::json palette_wire_json(const std::optional<Palette>& palette) {
    if (!palette.has_value()) {
        return nullptr;
    }
    nlohmann::json stops = nlohmann::json::array();
    for (const PaletteStop& stop : palette->stops) {
        stops.push_back(nlohmann::json{{"position", stop.position}, {"color", color_wire_json(stop.color)}});
    }
    return nlohmann::json{{"stops", stops}};
}

PanelMessage set_palette_message(std::uint64_t layer_id, const std::optional<Palette>& palette) {
    nlohmann::json doc{{"type", "set_palette_override"},
                       {"layer_id", layer_id},
                       {"palette_override", palette_wire_json(palette)}};
    std::optional<PanelMessage> message = panel_message::parse(doc.dump());
    EXPECT_TRUE(message.has_value());
    return message.value_or(PanelMessage{RequestStateMessage{}});
}

std::optional<Palette> pushed_palette_override(const nlohmann::json& pushed_layer) {
    const nlohmann::json& field = pushed_layer.at("palette_override");
    return field.is_null() ? std::nullopt : std::optional<Palette>(layer_preset::palette_from_json(field));
}

class PanelMessageApplyTest : public PanelApplyTestFixture {};

}

TEST_F(PanelMessageApplyTest, RequestStatePushesLayerStackPresetListAndWindowState) {
    apply(PanelMessage{RequestStateMessage{}});

    ASSERT_EQ(ui_host_.sent_messages.size(), 3u);
    EXPECT_EQ(sent_of_kind(ui_host_, "layer_stack")["kind"], "layer_stack");
    EXPECT_EQ(sent_of_kind(ui_host_, "preset_list")["kind"], "preset_list");
    EXPECT_EQ(sent_of_kind(ui_host_, "window_state")["kind"], "window_state");
}

TEST_F(PanelMessageApplyTest, AddLayerAddsALayerAndPushesLayerStack) {
    apply(PanelMessage{AddLayerMessage{VisualType::Spectrum}});

    ASSERT_EQ(compositor_.layers().size(), 1u);
    EXPECT_EQ(compositor_.layers()[0].visual_type, VisualType::Spectrum);
    nlohmann::json state = last_sent(ui_host_);
    EXPECT_EQ(state["kind"], "layer_stack");
    EXPECT_EQ(state["state"]["layers"].size(), 1u);
}

TEST_F(PanelMessageApplyTest, AddLayerAssignsANonWhiteDistinguishingTint) {
    apply(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    apply(PanelMessage{AddLayerMessage{VisualType::Spectrum}});

    ASSERT_EQ(compositor_.layers().size(), 2u);
    EXPECT_NE(compositor_.layers()[0].tint_color.g, WHITE.g);
    EXPECT_NE(compositor_.layers()[1].tint_color.g, WHITE.g);
    EXPECT_NE(compositor_.layers()[0].tint_color.g, compositor_.layers()[1].tint_color.g);
}

TEST_F(PanelMessageApplyTest, AddLayerAtCapSendsFailureStatusAndDoesNotGrow) {
    for (std::size_t i = 0; i < Compositor::kMaxLayers; ++i) {
        compositor_.add_layer(VisualType::Waveform);
    }

    apply(PanelMessage{AddLayerMessage{VisualType::Waveform}});

    EXPECT_EQ(compositor_.layers().size(), Compositor::kMaxLayers);
    nlohmann::json status = sent_of_kind(ui_host_, "status");
    EXPECT_EQ(status["ok"], false);
}

TEST_F(PanelMessageApplyTest, RemoveLayerRemovesByLayerId) {
    compositor_.add_layer(VisualType::Waveform);
    std::uint64_t target_id = compositor_.layers()[0].layer_id;
    compositor_.add_layer(VisualType::Spectrum);

    apply(PanelMessage{RemoveLayerMessage{target_id}});

    ASSERT_EQ(compositor_.layers().size(), 1u);
    EXPECT_EQ(compositor_.layers()[0].visual_type, VisualType::Spectrum);
}

TEST_F(PanelMessageApplyTest, RemoveLayerWithUnknownIdIsNoOpButStillPushesState) {
    compositor_.add_layer(VisualType::Waveform);

    apply(PanelMessage{RemoveLayerMessage{999999}});

    EXPECT_EQ(compositor_.layers().size(), 1u);
    EXPECT_EQ(last_sent(ui_host_)["kind"], "layer_stack");
}

TEST_F(PanelMessageApplyTest, RemoveLayerReassignsTintsOfRemainingLayersByNewIndex) {
    apply(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    apply(PanelMessage{AddLayerMessage{VisualType::Spectrum}});
    apply(PanelMessage{AddLayerMessage{VisualType::Radial}});
    std::uint64_t first_id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{RemoveLayerMessage{first_id}});

    ASSERT_EQ(compositor_.layers().size(), 2u);
    EXPECT_EQ(compositor_.layers()[0].tint_color.g, compositor_math::layer_palette_color(0).g);
    EXPECT_EQ(compositor_.layers()[1].tint_color.g, compositor_math::layer_palette_color(1).g);
}

TEST_F(PanelMessageApplyTest, DuplicateLayerAppendsACopy) {
    compositor_.add_layer(VisualType::Spectrum);
    std::uint64_t source_id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{DuplicateLayerMessage{source_id}});

    ASSERT_EQ(compositor_.layers().size(), 2u);
    EXPECT_EQ(compositor_.layers()[1].visual_type, VisualType::Spectrum);
    EXPECT_NE(compositor_.layers()[1].layer_id, source_id);
    EXPECT_EQ(compositor_.layers()[1].tint_color.g, compositor_math::layer_palette_color(1).g);
}

TEST_F(PanelMessageApplyTest, DuplicateLayerAtCapSendsFailureStatus) {
    for (std::size_t i = 0; i < Compositor::kMaxLayers; ++i) {
        compositor_.add_layer(VisualType::Waveform);
    }
    std::uint64_t source_id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{DuplicateLayerMessage{source_id}});

    EXPECT_EQ(compositor_.layers().size(), Compositor::kMaxLayers);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}

TEST_F(PanelMessageApplyTest, MoveLayerReordersByLayerId) {
    compositor_.add_layer(VisualType::Waveform);
    compositor_.add_layer(VisualType::Spectrum);
    std::uint64_t second_id = compositor_.layers()[1].layer_id;

    apply(PanelMessage{MoveLayerMessage{second_id, 0}});

    EXPECT_EQ(compositor_.layers()[0].visual_type, VisualType::Spectrum);
    EXPECT_EQ(compositor_.layers()[1].visual_type, VisualType::Waveform);
}

TEST_F(PanelMessageApplyTest, SetOpacityClampsOutOfRangeValue) {
    compositor_.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetLayerOpacityMessage{id, 5.0f}});

    EXPECT_FLOAT_EQ(compositor_.layers()[0].opacity, 1.0f);
}

TEST_F(PanelMessageApplyTest, SetBlendModeUpdatesField) {
    compositor_.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetLayerBlendModeMessage{id, LayerBlendMode::Screen}});

    EXPECT_EQ(compositor_.layers()[0].blend_mode, LayerBlendMode::Screen);
}

TEST_F(PanelMessageApplyTest, SetFreqBandUpdatesFieldIncludingClearingToGlobal) {
    compositor_.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetLayerFreqBandMessage{id, std::optional<std::size_t>(4)}});
    EXPECT_EQ(compositor_.layers()[0].freq_band, std::optional<std::size_t>(4));

    apply(PanelMessage{SetLayerFreqBandMessage{id, std::nullopt}});
    EXPECT_EQ(compositor_.layers()[0].freq_band, std::nullopt);
}

TEST_F(PanelMessageApplyTest, SetReactivitySourceUpdatesField) {
    compositor_.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetLayerReactivitySourceMessage{id, ReactivitySource::BandAmplitude}});

    EXPECT_EQ(compositor_.layers()[0].reactivity_source, ReactivitySource::BandAmplitude);
}

TEST_F(PanelMessageApplyTest, SetEnabledUpdatesField) {
    compositor_.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetLayerEnabledMessage{id, false}});

    EXPECT_FALSE(compositor_.layers()[0].enabled);
}

TEST_F(PanelMessageApplyTest, SetSensitivityClampsOutOfRangeValue) {
    compositor_.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetLayerSensitivityMessage{id, 99.0f}});

    EXPECT_FLOAT_EQ(compositor_.layers()[0].sensitivity, 4.0f);
}

TEST_F(PanelMessageApplyTest, SetVisualParamUpdatesParticlesSpawnRatePerFrame) {
    compositor_.add_layer(VisualType::Particles);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetVisualParamMessage{id, "spawn_rate_per_frame", 6.0f}});

    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor_.layers()[0].visual);
    EXPECT_FLOAT_EQ(particles.params().spawn_rate_per_frame, 6.0f);
}

TEST_F(PanelMessageApplyTest, SetVisualParamClampsParticlesLifetimeFrames) {
    compositor_.add_layer(VisualType::Particles);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetVisualParamMessage{id, "lifetime_frames", 999999.0f}});

    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor_.layers()[0].visual);
    EXPECT_EQ(particles.params().lifetime_frames, visual_config::kParticlesLifetimeFramesMax);
}

TEST_F(PanelMessageApplyTest, SetVisualParamWithUnknownNameSendsFailureStatus) {
    compositor_.add_layer(VisualType::Particles);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetVisualParamMessage{id, "not_a_real_param", 1.0f}});

    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}

TEST_F(PanelMessageApplyTest, SetVisualParamBoolReachesPanelAsJsonBooleanNotNumber) {
    compositor_.add_layer(VisualType::FlowField);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetVisualParamMessage{id, "ripple_on_onset_enabled", false}});

    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor_.layers()[0].visual);
    EXPECT_FALSE(flow.params().ripple_on_onset_enabled);
    nlohmann::json params = sent_of_kind(ui_host_, "layer_stack")["state"]["layers"][0]["params"];
    ASSERT_TRUE(params["ripple_on_onset_enabled"].is_boolean());
    EXPECT_EQ(params["ripple_on_onset_enabled"], false);
    ASSERT_TRUE(params["burst_on_onset_enabled"].is_boolean());
    EXPECT_EQ(params["burst_on_onset_enabled"], true);
}

TEST_F(PanelMessageApplyTest, SetVisualParamWithWrongValueTypeSendsFailureStatusAndLeavesParamUnchanged) {
    compositor_.add_layer(VisualType::FlowField);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(PanelMessage{SetVisualParamMessage{id, "ripple_on_onset_enabled", 0.0f}});

    auto& flow = dynamic_cast<FlowFieldVisual&>(*compositor_.layers()[0].visual);
    EXPECT_TRUE(flow.params().ripple_on_onset_enabled);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);

    ui_host_.sent_messages.clear();
    apply(PanelMessage{SetVisualParamMessage{id, "spawn_rate_per_frame", true}});

    EXPECT_FLOAT_EQ(flow.params().spawn_rate_per_frame, visual_config::kFlowFieldDefaultSpawnRatePerFrame);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}

TEST_F(PanelMessageApplyTest, SetVisualParamWithUnknownLayerIdIsANoOp) {
    compositor_.add_layer(VisualType::Particles);

    apply(PanelMessage{SetVisualParamMessage{424242, "spawn_rate_per_frame", 1.0f}});

    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor_.layers()[0].visual);
    EXPECT_FLOAT_EQ(particles.params().spawn_rate_per_frame, 2.0f);
}

TEST_F(PanelMessageApplyTest, UnknownLayerIdIsANoOpForEverySetter) {
    compositor_.add_layer(VisualType::Waveform);

    apply(PanelMessage{SetLayerOpacityMessage{424242, 0.1f}});

    EXPECT_FLOAT_EQ(compositor_.layers()[0].opacity, 1.0f);
}

TEST_F(PanelMessageApplyTest, SetDockedToCurrentValueIsASafeNoOp) {
    ASSERT_EQ(window_layout_.mode(), WindowMode::Docked);

    apply(PanelMessage{SetDockedMessage{true}});

    EXPECT_EQ(window_layout_.mode(), WindowMode::Docked);
    EXPECT_EQ(sent_of_kind(ui_host_, "window_state")["docked"], true);
}

TEST_F(PanelMessageApplyTest, SetDockedFalseDetaches) {
    apply(PanelMessage{SetDockedMessage{false}});

    EXPECT_EQ(window_layout_.mode(), WindowMode::Detached);
    EXPECT_EQ(sent_of_kind(ui_host_, "window_state")["docked"], false);
}

TEST_F(PanelMessageApplyTest, SetFullscreenToCurrentValueIsASafeNoOp) {
    ASSERT_FALSE(window_layout_.is_fullscreen());

    apply(PanelMessage{SetFullscreenMessage{false}});

    EXPECT_FALSE(window_layout_.is_fullscreen());
    EXPECT_EQ(sent_of_kind(ui_host_, "window_state")["fullscreen"], false);
}

TEST_F(PanelMessageApplyTest, SwitchSourceWithNoSourceSelectedFailsAndPushesFailureStatus) {
    ASSERT_FALSE(audio_source_.has_source());

    apply(PanelMessage{SwitchSourceMessage{}});

    EXPECT_FALSE(audio_source_.has_source());
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
    EXPECT_EQ(sent_of_kind(ui_host_, "window_state")["source"], "none");
}

TEST_F(PanelMessageApplyTest, EnableLoopbackSwitchesToLoopbackAndPushesWindowState) {
    apply(PanelMessage{EnableLoopbackMessage{}});

    EXPECT_TRUE(audio_source_.is_loopback());
    EXPECT_EQ(sent_of_kind(ui_host_, "window_state")["source"], "loopback");
}

TEST_F(PanelMessageApplyTest, SwitchSourceAfterEnablingLoopbackFailsWithNoFileToSwitchBackTo) {
    apply(PanelMessage{EnableLoopbackMessage{}});
    ASSERT_TRUE(audio_source_.is_loopback());

    apply(PanelMessage{SwitchSourceMessage{}});

    EXPECT_TRUE(audio_source_.is_loopback());
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}

namespace {

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

class PanelMessageApplyImportFileTest : public PanelMessageApplyTest {
protected:
    void SetUp() override {
        wav_path_ = std::filesystem::temp_directory_path() / "aviz_panel_apply_import_test.wav";
        std::vector<std::uint8_t> bytes = make_valid_wav(44100, 2, 1000);
        std::ofstream f(wav_path_, std::ios::binary);
        f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    void TearDown() override {
        producer_host_.switch_to(nullptr);
        std::filesystem::remove(wav_path_);
    }

    std::filesystem::path wav_path_;
};

TEST_F(PanelMessageApplyImportFileTest, ImportFileSwitchesToFileAndPushesSuccessStatus) {
    apply(PanelMessage{ImportFileMessage{wav_path_.string()}});

    EXPECT_EQ(audio_source_.kind(), AudioSourceKind::File);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], true);
    EXPECT_EQ(sent_of_kind(ui_host_, "window_state")["source"], "file");
}

TEST_F(PanelMessageApplyImportFileTest, ImportFileOnMissingFileSendsFailureStatusAndLeavesSourceUnchanged) {
    apply(PanelMessage{ImportFileMessage{"nonexistent.wav"}});

    EXPECT_EQ(audio_source_.kind(), AudioSourceKind::None);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}

TEST_F(PanelMessageApplyTest, SetPaletteOverrideSetsLayerAndPushesItToThePanel) {
    compositor_.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(set_palette_message(id, palette_fixtures::sunset()));

    palette_fixtures::expect_same_stops(compositor_.layers()[0].palette_override, palette_fixtures::sunset());
    nlohmann::json state = last_sent(ui_host_);
    EXPECT_EQ(state["kind"], "layer_stack");
    palette_fixtures::expect_same_stops(pushed_palette_override(state["state"]["layers"][0]),
                                        palette_fixtures::sunset());
}

TEST_F(PanelMessageApplyTest, SetPaletteOverrideNullClearsAndPushesNullToThePanel) {
    compositor_.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor_.layers()[0].layer_id;
    compositor_.layers()[0].palette_override = palette_fixtures::warm_cool();

    apply(set_palette_message(id, std::nullopt));

    EXPECT_FALSE(compositor_.layers()[0].palette_override.has_value());
    EXPECT_TRUE(last_sent(ui_host_)["state"]["layers"][0]["palette_override"].is_null());
}

TEST_F(PanelMessageApplyTest, SetPaletteOverrideOnlyChangesTheTargetLayer) {
    compositor_.add_layer(VisualType::Waveform);
    compositor_.add_layer(VisualType::Spectrum);
    std::uint64_t second_id = compositor_.layers()[1].layer_id;

    apply(set_palette_message(second_id, palette_fixtures::warm_cool()));

    EXPECT_FALSE(compositor_.layers()[0].palette_override.has_value());
    EXPECT_TRUE(compositor_.layers()[1].palette_override.has_value());
}

TEST_F(PanelMessageApplyTest, SetPaletteOverrideUnknownLayerChangesNothingButStillPushesState) {
    compositor_.add_layer(VisualType::Waveform);

    apply(set_palette_message(9999, palette_fixtures::warm_cool()));

    EXPECT_FALSE(compositor_.layers()[0].palette_override.has_value());
    EXPECT_EQ(last_sent(ui_host_)["kind"], "layer_stack");
}

TEST_F(PanelMessageApplyTest, SetPaletteOverrideLeavesStoredTintUntouched) {
    compositor_.add_layer(VisualType::Waveform);
    compositor_math::reassign_layer_tints(compositor_.layers());
    const Color tint_before = compositor_.layers()[0].tint_color;
    std::uint64_t id = compositor_.layers()[0].layer_id;

    apply(set_palette_message(id, palette_fixtures::sunset()));
    apply(set_palette_message(id, std::nullopt));

    EXPECT_EQ(compositor_.layers()[0].tint_color.r, tint_before.r);
    EXPECT_EQ(compositor_.layers()[0].tint_color.g, tint_before.g);
    EXPECT_EQ(compositor_.layers()[0].tint_color.b, tint_before.b);
}

TEST_F(PanelMessageApplyTest, DuplicateLayerKeepsPaletteOverrideSetThroughThePanel) {
    compositor_.add_layer(VisualType::Waveform);
    std::uint64_t id = compositor_.layers()[0].layer_id;
    apply(set_palette_message(id, palette_fixtures::fractional_position_odd_alpha()));

    apply(PanelMessage{DuplicateLayerMessage{id}});

    ASSERT_EQ(compositor_.layers().size(), 2u);
    palette_fixtures::expect_same_stops(compositor_.layers()[1].palette_override,
                                        palette_fixtures::fractional_position_odd_alpha());
}

class PanelMessageApplyPresetTest : public PanelMessageApplyTest {
protected:
    void SetUp() override { std::filesystem::remove_all(presets_dir_); }
    void TearDown() override { std::filesystem::remove_all(presets_dir_); }
};

TEST_F(PanelMessageApplyPresetTest, PaletteOverridesSetThroughPanelSurviveSaveAndLoadStopForStop) {
    const std::vector<std::optional<Palette>> palettes = palette_fixtures::all_cases();
    std::vector<std::uint64_t> ids;
    for (std::size_t i = 0; i < palettes.size(); ++i) {
        compositor_.add_layer(VisualType::Spectrum);
        ids.push_back(compositor_.layers()[i].layer_id);
        apply(set_palette_message(ids[i], palettes[i]));
    }
    apply(PanelMessage{SavePresetMessage{"palette-round-trip.json"}});
    ASSERT_EQ(sent_of_kind(ui_host_, "status")["ok"], true);

    for (std::size_t i = 0; i < palettes.size(); ++i) {
        apply(set_palette_message(ids[i], palettes[(i + 1) % palettes.size()]));
    }
    apply(PanelMessage{LoadPresetMessage{"palette-round-trip.json"}});

    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], true);
    ASSERT_EQ(compositor_.layers().size(), palettes.size());
    nlohmann::json pushed_layers = last_sent(ui_host_)["state"]["layers"];
    for (std::size_t i = 0; i < palettes.size(); ++i) {
        SCOPED_TRACE(i);
        EXPECT_EQ(compositor_.layers()[i].layer_id, ids[i]);
        palette_fixtures::expect_same_stops(compositor_.layers()[i].palette_override, palettes[i]);
        palette_fixtures::expect_same_stops(pushed_palette_override(pushed_layers[i]), palettes[i]);
    }
}

TEST_F(PanelMessageApplyPresetTest, SavePresetWritesFileAndSendsSuccessStatus) {
    compositor_.add_layer(VisualType::Waveform);

    apply(PanelMessage{SavePresetMessage{"apply-test.json"}});

    EXPECT_TRUE(std::filesystem::exists(preset_file_io::preset_path(presets_dir_, "apply-test.json")));
    nlohmann::json status = sent_of_kind(ui_host_, "status");
    EXPECT_EQ(status["ok"], true);
    nlohmann::json preset_list = sent_of_kind(ui_host_, "preset_list");
    EXPECT_NE(std::find(preset_list["files"].begin(), preset_list["files"].end(), "apply-test.json"),
              preset_list["files"].end());
}

TEST_F(PanelMessageApplyPresetTest, LoadPresetReplacesLayerStackAndSendsSuccessStatus) {
    Compositor writer;
    writer.add_layer(VisualType::Fractal);
    writer.add_layer(VisualType::Particles);
    FakeUIHost writer_host;
    AudioRingBuffer writer_ring(audio_config::kRingBufferCapacityFrames, audio_config::kPipelineChannels);
    ProducerHost writer_producer_host(writer_ring, std::make_unique<::testing::NiceMock<MockProducer>>());
    AudioSourceController writer_audio_source(writer_producer_host);
    WindowLayoutController writer_window_layout(writer, writer_host);
    panel_message_apply::PanelApplyContext writer_ctx{writer, writer_host, writer_window_layout,
                                                        writer_audio_source, presets_dir_};
    panel_message_apply::apply(PanelMessage{SavePresetMessage{"apply-load-test.json"}}, writer_ctx);

    compositor_.add_layer(VisualType::Waveform);

    apply(PanelMessage{LoadPresetMessage{"apply-load-test.json"}});

    ASSERT_EQ(compositor_.layers().size(), 2u);
    EXPECT_EQ(compositor_.layers()[0].visual_type, VisualType::Fractal);
    EXPECT_EQ(compositor_.layers()[1].visual_type, VisualType::Particles);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], true);
}

TEST_F(PanelMessageApplyPresetTest, LoadPresetOnMissingFileSendsFailureStatusAndLeavesStackUnchanged) {
    compositor_.add_layer(VisualType::Waveform);

    apply(PanelMessage{LoadPresetMessage{"does-not-exist.json"}});

    ASSERT_EQ(compositor_.layers().size(), 1u);
    EXPECT_EQ(compositor_.layers()[0].visual_type, VisualType::Waveform);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}

TEST_F(PanelMessageApplyPresetTest, LoadPresetOnMalformedFileSendsFailureStatusAndLeavesStackUnchanged) {
    preset_file_io::write_file(preset_file_io::preset_path(presets_dir_, "malformed.json"), "{ not valid json");
    compositor_.add_layer(VisualType::Waveform);

    apply(PanelMessage{LoadPresetMessage{"malformed.json"}});

    ASSERT_EQ(compositor_.layers().size(), 1u);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}

TEST_F(PanelMessageApplyTest, ExtremeJsonNumbersEndUpClampedToEachFieldsOwnRangeEndToEnd) {
    compositor_.add_layer(VisualType::FlowField);
    std::uint64_t id = compositor_.layers()[0].layer_id;
    const std::string prefix = R"({"layer_id":)" + std::to_string(id) + ",";

    for (const std::string& body :
         {std::string(R"("type":"set_opacity","opacity":1e300})"),
          std::string(R"("type":"set_sensitivity","sensitivity":1e300})"),
          std::string(R"("type":"set_visual_param","param_name":"spawn_rate_per_frame","value":1e300})"),
          std::string(R"("type":"set_visual_param","param_name":"turbulence_frequency","value":-1e300})"),
          std::string(R"("type":"set_visual_param","param_name":"evolution_per_frame","value":1e300})"),
          std::string(R"("type":"set_visual_param","param_name":"grid_cols","value":1e300})"),
          std::string(R"("type":"set_visual_param","param_name":"lifetime_frames","value":-1e300})")}) {
        std::optional<PanelMessage> parsed = panel_message::parse(prefix + body);
        ASSERT_TRUE(parsed.has_value()) << body;
        apply(*parsed);
    }

    const Layer& layer = compositor_.layers()[0];
    const FlowFieldParams& params = dynamic_cast<FlowFieldVisual&>(*layer.visual).params();
    EXPECT_FLOAT_EQ(layer.opacity, 1.0f);
    EXPECT_FLOAT_EQ(layer.sensitivity, compositor_math::kSensitivityMax);
    EXPECT_FLOAT_EQ(params.spawn_rate_per_frame, visual_config::kFlowFieldSpawnRatePerFrameMax);
    EXPECT_FLOAT_EQ(params.turbulence_frequency, visual_config::kFlowFieldTurbulenceFrequencyMin);
    EXPECT_FLOAT_EQ(params.evolution_per_frame, visual_config::kFlowFieldEvolutionPerFrameMax);
    EXPECT_EQ(params.grid_cols, visual_config::kFlowFieldGridColsMax);
    EXPECT_EQ(params.lifetime_frames, visual_config::kParticlesLifetimeFramesMin);
}

TEST_F(PanelMessageApplyTest, LissajousCompressionExponentFromPanelJsonIsAppliedClampedAndPushedBack) {
    compositor_.add_layer(VisualType::Lissajous);
    std::uint64_t id = compositor_.layers()[0].layer_id;
    const std::string prefix =
        R"({"type":"set_visual_param","param_name":"compression_exponent","layer_id":)" + std::to_string(id) +
        R"(,"value":)";
    auto& lissajous = dynamic_cast<LissajousVisual&>(*compositor_.layers()[0].visual);

    std::optional<PanelMessage> in_range = panel_message::parse(prefix + "0.75}");
    ASSERT_TRUE(in_range.has_value());
    apply(*in_range);
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, 0.75f);
    nlohmann::json params = sent_of_kind(ui_host_, "layer_stack")["state"]["layers"][0]["params"];
    EXPECT_FLOAT_EQ(params["compression_exponent"].get<float>(), 0.75f);

    std::optional<PanelMessage> high = panel_message::parse(prefix + "1e300}");
    ASSERT_TRUE(high.has_value());
    apply(*high);
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, visual_config::kLissajousCompressionExponentMax);

    std::optional<PanelMessage> low = panel_message::parse(prefix + "-1e300}");
    ASSERT_TRUE(low.has_value());
    apply(*low);
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, visual_config::kLissajousCompressionExponentMin);

    ui_host_.sent_messages.clear();
    std::optional<PanelMessage> wrong_type = panel_message::parse(prefix + "true}");
    ASSERT_TRUE(wrong_type.has_value());
    apply(*wrong_type);
    EXPECT_FLOAT_EQ(lissajous.params().compression_exponent, visual_config::kLissajousCompressionExponentMin);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}

TEST_F(PanelMessageApplyTest, RadialRadiusFractionsFromPanelJsonStopAtTheGapAndPushTheClampedValueBack) {
    compositor_.add_layer(VisualType::Radial);
    std::uint64_t id = compositor_.layers()[0].layer_id;
    auto& radial = dynamic_cast<RadialVisual&>(*compositor_.layers()[0].visual);
    auto send = [&](const std::string& param_name, const std::string& value) {
        std::optional<PanelMessage> parsed = panel_message::parse(
            R"({"type":"set_visual_param","layer_id":)" + std::to_string(id) + R"(,"param_name":")" + param_name +
            R"(","value":)" + value + "}");
        ASSERT_TRUE(parsed.has_value()) << param_name << " " << value;
        ui_host_.sent_messages.clear();
        apply(*parsed);
    };

    send("inner_radius_fraction", "1e300");
    EXPECT_NEAR(radial.params().inner_radius_fraction, 0.9f - radial_math::kMinRadiusGapFraction, 1e-6f);
    EXPECT_FLOAT_EQ(radial.params().max_radius_fraction, 0.9f);
    nlohmann::json params = sent_of_kind(ui_host_, "layer_stack")["state"]["layers"][0]["params"];
    EXPECT_NEAR(params["inner_radius_fraction"].get<float>(), 0.9f - radial_math::kMinRadiusGapFraction, 1e-6f);
    EXPECT_FLOAT_EQ(params["max_radius_fraction"].get<float>(), 0.9f);

    send("inner_radius_fraction", "0.2");
    send("max_radius_fraction", "-1e300");
    EXPECT_FLOAT_EQ(radial.params().inner_radius_fraction, 0.2f);
    EXPECT_NEAR(radial.params().max_radius_fraction, 0.2f + radial_math::kMinRadiusGapFraction, 1e-6f);

    send("max_radius_fraction", "true");
    EXPECT_NEAR(radial.params().max_radius_fraction, 0.2f + radial_math::kMinRadiusGapFraction, 1e-6f);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}

TEST_F(PanelMessageApplyTest, ParticlesInnerRadiusFractionFromPanelJsonIsAppliedClampedAndPushedBack) {
    compositor_.add_layer(VisualType::Particles);
    std::uint64_t id = compositor_.layers()[0].layer_id;
    const std::string prefix =
        R"({"type":"set_visual_param","param_name":"inner_radius_fraction","layer_id":)" + std::to_string(id) +
        R"(,"value":)";
    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor_.layers()[0].visual);

    std::optional<PanelMessage> in_range = panel_message::parse(prefix + "0.25}");
    ASSERT_TRUE(in_range.has_value());
    apply(*in_range);
    EXPECT_FLOAT_EQ(particles.params().inner_radius_fraction, 0.25f);
    nlohmann::json params = sent_of_kind(ui_host_, "layer_stack")["state"]["layers"][0]["params"];
    EXPECT_FLOAT_EQ(params["inner_radius_fraction"].get<float>(), 0.25f);

    std::optional<PanelMessage> high = panel_message::parse(prefix + "1e300}");
    ASSERT_TRUE(high.has_value());
    apply(*high);
    EXPECT_FLOAT_EQ(particles.params().inner_radius_fraction, 1.0f);

    std::optional<PanelMessage> low = panel_message::parse(prefix + "-1e300}");
    ASSERT_TRUE(low.has_value());
    apply(*low);
    EXPECT_FLOAT_EQ(particles.params().inner_radius_fraction, 0.0f);
}

TEST_F(PanelMessageApplyTest, FractalParamsFromPanelJsonAreAppliedClampedAndPushedBack) {
    compositor_.add_layer(VisualType::Fractal);
    std::uint64_t id = compositor_.layers()[0].layer_id;
    auto message_for = [id](const std::string& param_name, const std::string& value) {
        return R"({"type":"set_visual_param","param_name":")" + param_name + R"(","layer_id":)" +
               std::to_string(id) + R"(,"value":)" + value + "}";
    };
    auto& fractal = dynamic_cast<FractalVisual&>(*compositor_.layers()[0].visual);

    std::optional<PanelMessage> in_range = panel_message::parse(message_for("base_zoom", "2.5"));
    ASSERT_TRUE(in_range.has_value());
    apply(*in_range);
    EXPECT_FLOAT_EQ(fractal.params().base_zoom, 2.5f);
    nlohmann::json params = sent_of_kind(ui_host_, "layer_stack")["state"]["layers"][0]["params"];
    EXPECT_FLOAT_EQ(params["base_zoom"].get<float>(), 2.5f);

    std::optional<PanelMessage> high = panel_message::parse(message_for("julia_base_radius", "1e300"));
    ASSERT_TRUE(high.has_value());
    apply(*high);
    EXPECT_FLOAT_EQ(fractal.params().julia_base_radius, visual_config::kFractalJuliaBaseRadiusMax);

    std::optional<PanelMessage> low = panel_message::parse(message_for("base_zoom", "-1e300"));
    ASSERT_TRUE(low.has_value());
    apply(*low);
    EXPECT_FLOAT_EQ(fractal.params().base_zoom, visual_config::kFractalBaseZoomMin);

    std::optional<PanelMessage> toggle = panel_message::parse(message_for("julia_orbit_clockwise", "false"));
    ASSERT_TRUE(toggle.has_value());
    apply(*toggle);
    EXPECT_FALSE(fractal.params().julia_orbit_clockwise);
    params = sent_of_kind(ui_host_, "layer_stack")["state"]["layers"][0]["params"];
    EXPECT_TRUE(params["julia_orbit_clockwise"].is_boolean());
    EXPECT_EQ(params["julia_orbit_clockwise"], false);

    ui_host_.sent_messages.clear();
    std::optional<PanelMessage> wrong_type = panel_message::parse(message_for("julia_orbit_clockwise", "1"));
    ASSERT_TRUE(wrong_type.has_value());
    apply(*wrong_type);
    EXPECT_FALSE(fractal.params().julia_orbit_clockwise);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}

TEST_F(PanelMessageApplyImportFileTest, NativeFileDialogImportReachesTheAudioSourceThroughTheDispatchQueue) {
    PanelDispatchQueue queue;
    queue.push(PanelMessage{ImportFileMessage{wav_path_.string()}});

    for (const PanelMessage& message : queue.drain()) {
        apply(message);
    }

    EXPECT_EQ(audio_source_.kind(), AudioSourceKind::File);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], true);
    EXPECT_EQ(sent_of_kind(ui_host_, "window_state")["source"], "file");
}

TEST_F(PanelMessageApplyImportFileTest, ImportFileJsonFromThePanelChannelNeverReachesTheAudioSource) {
    nlohmann::json text{{"type", "import_file"}, {"path", wav_path_.string()}};
    PanelDispatchQueue queue;

    std::optional<PanelMessage> parsed = panel_message::parse(text.dump());
    if (parsed.has_value()) {
        queue.push(*parsed);
    }
    for (const PanelMessage& message : queue.drain()) {
        apply(message);
    }

    EXPECT_FALSE(parsed.has_value());
    EXPECT_EQ(audio_source_.kind(), AudioSourceKind::None);
    EXPECT_TRUE(ui_host_.sent_messages.empty());
}

TEST_F(PanelMessageApplyPresetTest, SaveListAndLoadUseThePresetsDirectoryNotTheWorkingDirectory) {
    const std::filesystem::path original_working_dir = std::filesystem::current_path();
    const std::filesystem::path unrelated_working_dir =
        std::filesystem::temp_directory_path() / "aviz_panel_apply_unrelated_working_dir";
    std::filesystem::remove_all(unrelated_working_dir);
    std::filesystem::create_directories(unrelated_working_dir);
    std::filesystem::current_path(unrelated_working_dir);

    compositor_.add_layer(VisualType::Spectrum);
    apply(PanelMessage{SavePresetMessage{"cwd-test.json"}});
    const bool saved_in_presets_dir = std::filesystem::exists(std::filesystem::path(presets_dir_) / "cwd-test.json");
    const bool saved_under_working_dir = std::filesystem::exists(
        unrelated_working_dir / preset_file_io::kPresetsDirectoryName / "cwd-test.json");
    const nlohmann::json preset_list = sent_of_kind(ui_host_, "preset_list");
    compositor_.remove_layer(0);
    ui_host_.sent_messages.clear();
    apply(PanelMessage{LoadPresetMessage{"cwd-test.json"}});
    const nlohmann::json load_status = sent_of_kind(ui_host_, "status");

    std::filesystem::current_path(original_working_dir);
    std::filesystem::remove_all(unrelated_working_dir);

    EXPECT_TRUE(saved_in_presets_dir);
    EXPECT_FALSE(saved_under_working_dir);
    EXPECT_NE(std::find(preset_list["files"].begin(), preset_list["files"].end(), "cwd-test.json"),
              preset_list["files"].end());
    EXPECT_EQ(load_status["ok"], true);
    ASSERT_EQ(compositor_.layers().size(), 1u);
    EXPECT_EQ(compositor_.layers()[0].visual_type, VisualType::Spectrum);
}
