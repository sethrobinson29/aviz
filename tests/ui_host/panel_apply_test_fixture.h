#pragma once

#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "app/audio_source_controller.h"
#include "app/window_layout_controller.h"
#include "audio/audio_config.h"
#include "audio/mock_producer.h"
#include "audio/producer_host.h"
#include "audio/ring_buffer.h"
#include "compositor/compositor.h"
#include "ui_host/fake_ui_host.h"
#include "ui_host/panel_message_apply.h"

class PanelApplyTestFixture : public ::testing::Test {
protected:
    PanelApplyTestFixture()
        : ring_(audio_config::kRingBufferCapacityFrames, audio_config::kPipelineChannels),
          producer_host_(ring_, std::make_unique<::testing::NiceMock<MockProducer>>()),
          audio_source_(producer_host_),
          window_layout_(compositor_, ui_host_),
          ctx_{compositor_, ui_host_, window_layout_, audio_source_} {}

    void apply(const PanelMessage& message) { panel_message_apply::apply(message, ctx_); }

    Compositor compositor_;
    FakeUIHost ui_host_;
    AudioRingBuffer ring_;
    ProducerHost producer_host_;
    AudioSourceController audio_source_;
    WindowLayoutController window_layout_;
    panel_message_apply::PanelApplyContext ctx_;
};
