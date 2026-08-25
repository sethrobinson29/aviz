#include "loopback_producer.h"

#include "miniaudio.h"

#include <stdexcept>

#include "pipeline_format_converter.h"

struct LoopbackProducer::Impl {
    ma_device device{};
    bool device_open = false;
    std::unique_ptr<PipelineFormatConverter> converter;
    AudioRingBuffer* ring = nullptr;
    unsigned native_channels = 0;
    unsigned native_sample_rate = 0;

    static void data_callback(ma_device* device, void* /*output*/, const void* input,
                               ma_uint32 frame_count) {
        auto* self = static_cast<Impl*>(device->pUserData);
        if (frame_count > 0 && self->ring != nullptr) {
            self->converter->convert_and_push(static_cast<const float*>(input),
                                               static_cast<std::size_t>(frame_count), *self->ring);
        }
    }
};

LoopbackProducer::LoopbackProducer() : impl_(std::make_unique<Impl>()) {
    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    config.capture.pDeviceID = nullptr;  // default output device's mix
    config.capture.format = ma_format_f32;
    config.capture.channels = 0;  // accept the device's native channel count
    config.sampleRate = 0;        // accept the device's native rate
    config.dataCallback = Impl::data_callback;
    config.pUserData = impl_.get();

    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) {
        throw std::runtime_error("LoopbackProducer: failed to open loopback capture device");
    }
    impl_->device_open = true;

    impl_->native_channels = impl_->device.capture.channels;
    impl_->native_sample_rate = impl_->device.sampleRate;
    impl_->converter =
        std::make_unique<PipelineFormatConverter>(impl_->native_channels, impl_->native_sample_rate);
}

LoopbackProducer::~LoopbackProducer() { stop(); }

unsigned LoopbackProducer::native_channels() const { return impl_->native_channels; }
unsigned LoopbackProducer::native_sample_rate() const { return impl_->native_sample_rate; }

void LoopbackProducer::start(AudioRingBuffer& ring) {
    impl_->ring = &ring;

    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        throw std::runtime_error("LoopbackProducer: failed to start loopback capture device");
    }
}

void LoopbackProducer::stop() {
    if (impl_->device_open) {
        ma_device_uninit(&impl_->device);
        impl_->device_open = false;
    }
}
