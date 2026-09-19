#include "ui_host/panel_dispatch_queue.h"

#include <vector>

#include <gtest/gtest.h>

#include "compositor/compositor.h"
#include "compositor/layer.h"

namespace {

void apply_drained_messages(PanelDispatchQueue& queue, Compositor& compositor) {
    for (const PanelMessage& message : queue.drain()) {
        if (message.type == PanelMessageType::AddLayer) {
            compositor.add_layer(VisualType::Waveform);
        }
    }
}

}

TEST(PanelDispatchRoundTripTest, AddLayerMessageAddsExactlyOneLayer) {
    PanelDispatchQueue queue;
    Compositor compositor;

    queue.push(PanelMessage{PanelMessageType::AddLayer});
    apply_drained_messages(queue, compositor);

    ASSERT_EQ(compositor.layers().size(), 1u);
    EXPECT_EQ(compositor.layers()[0].visual_type, VisualType::Waveform);
}

TEST(PanelDispatchRoundTripTest, MultipleQueuedMessagesApplyInOrder) {
    PanelDispatchQueue queue;
    Compositor compositor;

    queue.push(PanelMessage{PanelMessageType::AddLayer});
    queue.push(PanelMessage{PanelMessageType::AddLayer});
    queue.push(PanelMessage{PanelMessageType::AddLayer});
    apply_drained_messages(queue, compositor);

    EXPECT_EQ(compositor.layers().size(), 3u);
}

TEST(PanelDispatchRoundTripTest, DrainingTwiceDoesNotDoubleApply) {
    PanelDispatchQueue queue;
    Compositor compositor;

    queue.push(PanelMessage{PanelMessageType::AddLayer});
    apply_drained_messages(queue, compositor);
    apply_drained_messages(queue, compositor);

    EXPECT_EQ(compositor.layers().size(), 1u);
}

TEST(PanelDispatchRoundTripTest, RespectsLayerCap) {
    PanelDispatchQueue queue;
    Compositor compositor;

    for (std::size_t i = 0; i < Compositor::kMaxLayers + 5; ++i) {
        queue.push(PanelMessage{PanelMessageType::AddLayer});
    }
    apply_drained_messages(queue, compositor);

    EXPECT_EQ(compositor.layers().size(), Compositor::kMaxLayers);
}
