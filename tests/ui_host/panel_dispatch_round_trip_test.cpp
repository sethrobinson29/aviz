#include "ui_host/panel_dispatch_queue.h"

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "compositor/compositor.h"
#include "compositor/layer.h"
#include "compositor/layer_preset.h"
#include "ui_host/panel_apply_test_fixture.h"
#include "ui_host/panel_message.h"
#include "ui_host/panel_message_apply.h"
#include "visuals/flow_field/flow_field_visual.h"
#include "visuals/particles/particles_visual.h"

namespace {

class PanelDispatchRoundTripTest : public PanelApplyTestFixture {
protected:
    void apply_drained_messages() {
        for (const PanelMessage& message : queue_.drain()) {
            apply(message);
        }
    }

    PanelDispatchQueue queue_;
};

}

TEST_F(PanelDispatchRoundTripTest, AddLayerMessageAddsExactlyOneLayer) {
    queue_.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    apply_drained_messages();

    ASSERT_EQ(compositor_.layers().size(), 1u);
    EXPECT_EQ(compositor_.layers()[0].visual_type, VisualType::Waveform);
}

TEST_F(PanelDispatchRoundTripTest, MultipleQueuedMessagesApplyInOrder) {
    queue_.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    queue_.push(PanelMessage{AddLayerMessage{VisualType::Spectrum}});
    queue_.push(PanelMessage{AddLayerMessage{VisualType::Radial}});
    apply_drained_messages();

    ASSERT_EQ(compositor_.layers().size(), 3u);
    EXPECT_EQ(compositor_.layers()[0].visual_type, VisualType::Waveform);
    EXPECT_EQ(compositor_.layers()[1].visual_type, VisualType::Spectrum);
    EXPECT_EQ(compositor_.layers()[2].visual_type, VisualType::Radial);
}

TEST_F(PanelDispatchRoundTripTest, DrainingTwiceDoesNotDoubleApply) {
    queue_.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    apply_drained_messages();
    apply_drained_messages();

    EXPECT_EQ(compositor_.layers().size(), 1u);
}

TEST_F(PanelDispatchRoundTripTest, RespectsLayerCap) {
    for (std::size_t i = 0; i < Compositor::kMaxLayers + 5; ++i) {
        queue_.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    }
    apply_drained_messages();

    EXPECT_EQ(compositor_.layers().size(), Compositor::kMaxLayers);
}

TEST_F(PanelDispatchRoundTripTest, MixedMessageTypesApplyInPushOrder) {
    std::uint64_t first_id = compositor_.add_layer(VisualType::Waveform).has_value()
                                  ? compositor_.layers()[0].layer_id
                                  : 0;

    queue_.push(PanelMessage{AddLayerMessage{VisualType::Spectrum}});
    queue_.push(PanelMessage{SetLayerOpacityMessage{first_id, 0.25f}});
    queue_.push(PanelMessage{RemoveLayerMessage{first_id}});
    apply_drained_messages();

    ASSERT_EQ(compositor_.layers().size(), 1u);
    EXPECT_EQ(compositor_.layers()[0].visual_type, VisualType::Spectrum);
}

TEST_F(PanelDispatchRoundTripTest, SetDockedAndEnableLoopbackMessagesRouteThroughTheQueueToo) {
    queue_.push(PanelMessage{SetDockedMessage{false}});
    queue_.push(PanelMessage{EnableLoopbackMessage{}});
    apply_drained_messages();

    EXPECT_EQ(window_layout_.mode(), WindowMode::Detached);
    EXPECT_TRUE(audio_source_.is_loopback());
}

TEST_F(PanelDispatchRoundTripTest, SetVisualParamMessageRoutesThroughTheQueue) {
    std::uint64_t id = compositor_.add_layer(VisualType::Particles).has_value()
                            ? compositor_.layers()[0].layer_id
                            : 0;

    queue_.push(PanelMessage{SetVisualParamMessage{id, "spawn_rate_per_frame", 7.0f}});
    apply_drained_messages();

    auto& particles = dynamic_cast<ParticlesVisual&>(*compositor_.layers()[0].visual);
    EXPECT_FLOAT_EQ(particles.params().spawn_rate_per_frame, 7.0f);
}

TEST_F(PanelDispatchRoundTripTest, PanelBoolToggleSurvivesPresetSerializeDeserializeAsBoolean) {
    compositor_.add_layer(VisualType::FlowField);
    std::uint64_t id = compositor_.layers()[0].layer_id;
    std::optional<PanelMessage> parsed = panel_message::parse(
        R"({"type":"set_visual_param","layer_id":)" + std::to_string(id) +
        R"(,"param_name":"burst_on_onset_enabled","value":false})");
    ASSERT_TRUE(parsed.has_value());

    queue_.push(*parsed);
    apply_drained_messages();

    const std::string serialized = layer_preset::serialize(compositor_.layers());
    nlohmann::json doc = nlohmann::json::parse(serialized);
    ASSERT_TRUE(doc["layers"][0]["params"]["burst_on_onset_enabled"].is_boolean());
    EXPECT_EQ(doc["layers"][0]["params"]["burst_on_onset_enabled"], false);
    EXPECT_EQ(doc["layers"][0]["params"]["ripple_on_onset_enabled"], true);

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(serialized);
    ASSERT_TRUE(loaded.has_value());
    auto* flow = dynamic_cast<FlowFieldVisual*>((*loaded)[0].visual.get());
    ASSERT_NE(flow, nullptr);
    EXPECT_FALSE(flow->params().burst_on_onset_enabled);
    EXPECT_TRUE(flow->params().ripple_on_onset_enabled);
}

TEST_F(PanelDispatchRoundTripTest, ImportFileMessageRoutesThroughTheQueue) {
    queue_.push(PanelMessage{ImportFileMessage{"nonexistent-round-trip.wav"}});
    apply_drained_messages();

    EXPECT_EQ(audio_source_.kind(), AudioSourceKind::None);
}
