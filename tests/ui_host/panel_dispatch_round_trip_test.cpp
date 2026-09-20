#include "ui_host/panel_dispatch_queue.h"

#include <gtest/gtest.h>

#include "compositor/compositor.h"
#include "compositor/layer.h"
#include "ui_host/panel_apply_test_fixture.h"
#include "ui_host/panel_message_apply.h"

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

TEST_F(PanelDispatchRoundTripTest, SetDockedAndSwitchSourceMessagesRouteThroughTheQueueToo) {
    queue_.push(PanelMessage{SetDockedMessage{false}});
    queue_.push(PanelMessage{SwitchSourceMessage{}});
    apply_drained_messages();

    EXPECT_EQ(window_layout_.mode(), WindowMode::Detached);
    EXPECT_TRUE(audio_source_.is_loopback());
}
