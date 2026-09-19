#include "ui_host/panel_dispatch_queue.h"

#include <thread>
#include <vector>

#include <gtest/gtest.h>

TEST(PanelDispatchQueueTest, DrainOnEmptyQueueReturnsEmpty) {
    PanelDispatchQueue queue;
    EXPECT_TRUE(queue.drain().empty());
}

TEST(PanelDispatchQueueTest, DrainPreservesPushOrder) {
    PanelDispatchQueue queue;
    for (int i = 0; i < 5; ++i) {
        queue.push(PanelMessage{PanelMessageType::AddLayer});
    }

    std::vector<PanelMessage> drained = queue.drain();
    ASSERT_EQ(drained.size(), 5u);
    for (const PanelMessage& message : drained) {
        EXPECT_EQ(message.type, PanelMessageType::AddLayer);
    }
}

TEST(PanelDispatchQueueTest, DrainClearsTheQueue) {
    PanelDispatchQueue queue;
    queue.push(PanelMessage{PanelMessageType::AddLayer});

    ASSERT_EQ(queue.drain().size(), 1u);
    EXPECT_TRUE(queue.drain().empty());
}

TEST(PanelDispatchQueueTest, QueueIsUsableAgainAfterDraining) {
    PanelDispatchQueue queue;
    queue.push(PanelMessage{PanelMessageType::AddLayer});
    queue.drain();

    queue.push(PanelMessage{PanelMessageType::AddLayer});
    queue.push(PanelMessage{PanelMessageType::AddLayer});
    EXPECT_EQ(queue.drain().size(), 2u);
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
                queue.push(PanelMessage{PanelMessageType::AddLayer});
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(queue.drain().size(), static_cast<std::size_t>(kThreads) * kMessagesPerThread);
    EXPECT_TRUE(queue.drain().empty());
}
