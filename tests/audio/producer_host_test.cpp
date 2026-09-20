#include "audio/producer_host.h"

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "audio/audio_producer.h"
#include "audio/mock_producer.h"
#include "audio/ring_buffer.h"

TEST(ProducerHostTest, ConstructorStartsInitialProducer) {
    AudioRingBuffer ring(64, 2);
    auto initial = std::make_unique<MockProducer>();
    EXPECT_CALL(*initial, start(::testing::Ref(ring))).Times(1);

    ProducerHost host(ring, std::move(initial));
}

TEST(ProducerHostTest, SwitchToStopsOldProducerBeforeStartingNew) {
    AudioRingBuffer ring(64, 2);
    auto initial = std::make_unique<MockProducer>();
    MockProducer* initial_ptr = initial.get();
    EXPECT_CALL(*initial_ptr, start(::testing::_)).Times(1);

    ProducerHost host(ring, std::move(initial));

    auto next = std::make_unique<MockProducer>();
    MockProducer* next_ptr = next.get();

    ::testing::Sequence order;
    EXPECT_CALL(*initial_ptr, stop()).InSequence(order);
    EXPECT_CALL(*next_ptr, start(::testing::Ref(ring))).InSequence(order);

    host.switch_to(std::move(next));
}

TEST(ProducerHostTest, DestructorStopsCurrentProducer) {
    AudioRingBuffer ring(64, 2);
    auto initial = std::make_unique<MockProducer>();
    MockProducer* initial_ptr = initial.get();
    EXPECT_CALL(*initial_ptr, start(::testing::_)).Times(1);

    {
        ProducerHost host(ring, std::move(initial));
        EXPECT_CALL(*initial_ptr, stop()).Times(1);
    }
}
