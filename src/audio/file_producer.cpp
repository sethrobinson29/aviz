#include "file_producer.h"

#include "miniaudio.h"

#include <cstring>
#include <filesystem>
#include <stdexcept>

#include "pipeline_format_converter.h"

struct FileProducer::Impl {
    ma_decoder decoder{};
    ma_device device{};
    bool decoder_open = false;
    bool device_open = false;
    std::unique_ptr<PipelineFormatConverter> converter;
    AudioRingBuffer* ring = nullptr;
    std::atomic<bool> finished{false};

    static void data_callback(ma_device* device, void* output, const void*,
                               ma_uint32 frame_count) {
        auto* self = static_cast<Impl*>(device->pUserData);
        auto* out = static_cast<float*>(output);
        unsigned channels = device->playback.channels;

        ma_uint64 frames_read = 0;
        ma_decoder_read_pcm_frames(&self->decoder, out, frame_count, &frames_read);

        if (frames_read < frame_count) {
            std::memset(out + frames_read * channels, 0,
                        (frame_count - frames_read) * channels * sizeof(float));
            self->finished.store(true, std::memory_order_release);
        }
        if (frames_read > 0 && self->ring != nullptr) {
            self->converter->convert_and_push(out, static_cast<std::size_t>(frames_read), *self->ring);
        }
    }
};

FileProducer::FileProducer(const std::string& path) : impl_(std::make_unique<Impl>()) {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    const std::wstring wide_path = std::filesystem::u8path(path).wstring();
    if (ma_decoder_init_file_w(wide_path.c_str(), &config, &impl_->decoder) != MA_SUCCESS) {
        throw std::runtime_error("FileProducer: failed to open/decode file: " + path);
    }
    impl_->decoder_open = true;
}

FileProducer::~FileProducer() {
    stop();
    if (impl_->decoder_open) {
        ma_decoder_uninit(&impl_->decoder);
    }
}

unsigned FileProducer::native_channels() const { return impl_->decoder.outputChannels; }
unsigned FileProducer::native_sample_rate() const { return impl_->decoder.outputSampleRate; }

void FileProducer::start(AudioRingBuffer& ring) {
    impl_->ring = &ring;
    impl_->converter = std::make_unique<PipelineFormatConverter>(impl_->decoder.outputChannels,
                                                                   impl_->decoder.outputSampleRate);

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = impl_->decoder.outputFormat;
    config.playback.channels = impl_->decoder.outputChannels;
    config.sampleRate = impl_->decoder.outputSampleRate;
    config.dataCallback = Impl::data_callback;
    config.pUserData = impl_.get();

    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) {
        throw std::runtime_error("FileProducer: failed to open playback device");
    }
    impl_->device_open = true;

    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        ma_device_uninit(&impl_->device);
        impl_->device_open = false;
        throw std::runtime_error("FileProducer: failed to start playback device");
    }
}

void FileProducer::stop() {
    if (impl_->device_open) {
        ma_device_uninit(&impl_->device);
        impl_->device_open = false;
    }
}

bool FileProducer::is_finished() const {
    return impl_->finished.load(std::memory_order_acquire);
}
