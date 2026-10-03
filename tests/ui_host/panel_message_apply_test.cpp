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
#include "compositor/preset_file_io.h"
#include "ui_host/fake_ui_host.h"
#include "ui_host/panel_apply_test_fixture.h"
#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/particles/particles_visual.h"
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

class PanelMessageApplyPresetTest : public PanelMessageApplyTest {
protected:
    void SetUp() override { std::filesystem::remove_all(preset_file_io::kPresetsDirectoryName); }
    void TearDown() override { std::filesystem::remove_all(preset_file_io::kPresetsDirectoryName); }
};

TEST_F(PanelMessageApplyPresetTest, SavePresetWritesFileAndSendsSuccessStatus) {
    compositor_.add_layer(VisualType::Waveform);

    apply(PanelMessage{SavePresetMessage{"apply-test.json"}});

    EXPECT_TRUE(std::filesystem::exists(preset_file_io::preset_path("apply-test.json")));
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
                                                        writer_audio_source};
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
    preset_file_io::write_file(preset_file_io::preset_path("malformed.json"), "{ not valid json");
    compositor_.add_layer(VisualType::Waveform);

    apply(PanelMessage{LoadPresetMessage{"malformed.json"}});

    ASSERT_EQ(compositor_.layers().size(), 1u);
    EXPECT_EQ(sent_of_kind(ui_host_, "status")["ok"], false);
}
