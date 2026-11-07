#pragma once

#include <gmock/gmock.h>

#include "audio/audio_producer.h"
#include "audio/ring_buffer.h"

class MockProducer : public AudioProducer {
public:
    MOCK_METHOD(void, start, (AudioRingBuffer&), (override));
    MOCK_METHOD(void, stop, (), (override));
    MOCK_METHOD(unsigned, native_channels, (), (const, override));
    MOCK_METHOD(unsigned, native_sample_rate, (), (const, override));
};
