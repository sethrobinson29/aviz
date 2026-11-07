#include "ui_host/panel_dispatch_queue.h"

#include <thread>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

TEST(PanelDispatchQueueTest, DrainOnEmptyQueueReturnsEmpty) {
    PanelDispatchQueue queue;
    EXPECT_TRUE(queue.drain().empty());
}

TEST(PanelDispatchQueueTest, DrainPreservesPushOrder) {
    PanelDispatchQueue queue;
    for (int i = 0; i < 5; ++i) {
        queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    }

    std::vector<PanelMessage> drained = queue.drain();
    ASSERT_EQ(drained.size(), 5u);
    for (const PanelMessage& message : drained) {
        EXPECT_TRUE(std::holds_alternative<AddLayerMessage>(message));
    }
}

TEST(PanelDispatchQueueTest, DrainClearsTheQueue) {
    PanelDispatchQueue queue;
    queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});

    ASSERT_EQ(queue.drain().size(), 1u);
    EXPECT_TRUE(queue.drain().empty());
}

TEST(PanelDispatchQueueTest, QueueIsUsableAgainAfterDraining) {
    PanelDispatchQueue queue;
    queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    queue.drain();

    queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    EXPECT_EQ(queue.drain().size(), 2u);
}

TEST(PanelDispatchQueueTest, PreservesDistinctMessageTypesInOrder) {
    PanelDispatchQueue queue;
    queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
    queue.push(PanelMessage{RemoveLayerMessage{7}});
    queue.push(PanelMessage{RequestStateMessage{}});

    std::vector<PanelMessage> drained = queue.drain();
    ASSERT_EQ(drained.size(), 3u);
    EXPECT_TRUE(std::holds_alternative<AddLayerMessage>(drained[0]));
    ASSERT_TRUE(std::holds_alternative<RemoveLayerMessage>(drained[1]));
    EXPECT_EQ(std::get<RemoveLayerMessage>(drained[1]).layer_id, 7u);
    EXPECT_TRUE(std::holds_alternative<RequestStateMessage>(drained[2]));
}

TEST(PanelDispatchQueueTest, ConcurrentPushesAreAllObserved) {
    PanelDispatchQueue queue;
    constexpr int kThreads = 8;
    constexpr int kMessagesPerThread = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&queue, kMessagesPerThread] {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                queue.push(PanelMessage{AddLayerMessage{VisualType::Waveform}});
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(queue.drain().size(), static_cast<std::size_t>(kThreads) * kMessagesPerThread);
    EXPECT_TRUE(queue.drain().empty());
}
