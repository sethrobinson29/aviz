#include "audio/producer_host.h"

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "audio/audio_producer.h"
#include "audio/ring_buffer.h"

namespace {

class MockProducer : public AudioProducer {
public:
    MOCK_METHOD(void, start, (AudioRingBuffer&), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(unsigned, native_channels, (), (const, override));
    MOCK_METHOD(unsigned, native_sample_rate, (), (const, override));
};

}

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
