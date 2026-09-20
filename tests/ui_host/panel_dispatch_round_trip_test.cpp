#include "ui_host/panel_dispatch_queue.h"

#include <gtest/gtest.h>

#include "compositor/compositor.h"
#include "compositor/layer.h"
#include "ui_host/fake_ui_host.h"
#include "ui_host/panel_message_apply.h"

namespace {

void apply_drained_messages(PanelDispatchQueue& queue, Compositor& compositor, UIHost& ui_host) {
    for (const PanelMessage& message : queue.drain()) {
        panel_message_apply::apply(message, compositor, ui_host);
    }
}

}

TEST(PanelDispatchRoundTripTest, AddLayerMessageAddsExactlyOneLayer) {
    PanelDispatchQueue queue;
    Compositor compositor;
    FakeUIHost ui_host;

    queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    apply_drained_messages(queue, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Waveform);
}

TEST(PanelDispatchRoundTripTest, MultipleQueuedMessagesApplyInOrder) {
    PanelDispatchQueue queue;
    Compositor compositor;
    FakeUIHost ui_host;

    queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    queue.push(PanelMessage{AddLayerMessage{VisualType::Spectrum}});
    queue.push(PanelMessage{AddLayerMessage{VisualType::Radial}});
    apply_drained_messages(queue, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 3u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Waveform);
    EXPECT_EQ(compositor.layers()[1].visual_type, VisualType::Spectrum);
    EXPECT_EQ(compositor.layers()[2].visual_type, VisualType::Radial);
}

TEST(PanelDispatchRoundTripTest, DrainingTwiceDoesNotDoubleApply) {
    PanelDispatchQueue queue;
    Compositor compositor;
    FakeUIHost ui_host;

    queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    apply_drained_messages(queue, compositor, ui_host);
    apply_drained_messages(queue, compositor, ui_host);

    EXPECT_EQ(compositor.layers().size(), 1u);
}

TEST(PanelDispatchRoundTripTest, RespectsLayerCap) {
    PanelDispatchQueue queue;
    Compositor compositor;
    FakeUIHost ui_host;

    for (std::size_t i = 0; i < Compositor::kMaxLayers + 5; ++i) {
        queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    }
    apply_drained_messages(queue, compositor, ui_host);

    EXPECT_EQ(compositor.layers().size(), Compositor::kMaxLayers);
}

TEST(PanelDispatchRoundTripTest, MixedMessageTypesApplyInPushOrder) {
    PanelDispatchQueue queue;
    Compositor compositor;
    FakeUIHost ui_host;
    std::uint64_t first_id = compositor.add_layer(VisualType::Waveform).has_value()
                                  ? compositor.layers()[0].layer_id
                                  : 0;

    queue.push(PanelMessage{AddLayerMessage{VisualType::Spectrum}});
    queue.push(PanelMessage{SetLayerOpacityMessage{first_id, 0.25f}});
    queue.push(PanelMessage{RemoveLayerMessage{first_id}});
    apply_drained_messages(queue, compositor, ui_host);

    ASSERT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Spectrum);
}
