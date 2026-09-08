#include "pipeline_format_converter.h"

#include "miniaudio.h"

#include <cassert>
#include <stdexcept>
#include <vector>

#include "audio_config.h"
#include "ring_buffer.h"

namespace {
constexpr std::size_t kScratchOutputFrames = 8192;
}

struct PipelineFormatConverter::Impl {
    ma_data_converter converter{};
    std::vector<float> output_scratch;
    unsigned channels_in;

    explicit Impl(unsigned channels_in_)
        : output_scratch(kScratchOutputFrames * audio_config::kPipelineChannels),
          channels_in(channels_in_) {}
};

PipelineFormatConverter::PipelineFormatConverter(unsigned channels_in, unsigned sample_rate_in)
    : impl_(std::make_unique<Impl>(channels_in)) {
    ma_data_converter_config config = ma_data_converter_config_init(
        ma_format_f32, ma_format_f32, channels_in, audio_config::kPipelineChannels, sample_rate_in,
        audio_config::kPipelineSampleRate);
    config.resampling.algorithm = ma_resample_algorithm_linear;

    if (ma_data_converter_init(&config, nullptr, &impl_->converter) != MA_SUCCESS) {
        throw std::runtime_error("PipelineFormatConverter: failed to initialize converter");
    }
}

PipelineFormatConverter::~PipelineFormatConverter() { ma_data_converter_uninit(&impl_->converter, nullptr); }

void PipelineFormatConverter::convert_and_push(const float* interleaved_in, std::size_t frame_count_in,
                                                AudioRingBuffer& ring) {
    const float* in_cursor = interleaved_in;
    ma_uint64 frames_remaining_in = frame_count_in;

    while (frames_remaining_in > 0) {
        ma_uint64 frames_in = frames_remaining_in;
        ma_uint64 frames_out = kScratchOutputFrames;

        ma_data_converter_process_pcm_frames(&impl_->converter, in_cursor, &frames_in,
                                              impl_->output_scratch.data(), &frames_out);

        if (frames_out > 0) {
            ring.push_frames(impl_->output_scratch.data(), static_cast<std::size_t>(frames_out));
        }

        in_cursor += static_cast<std::size_t>(frames_in) * impl_->channels_in;
        frames_remaining_in -= frames_in;

        assert(!(frames_in == 0 && frames_out == 0));
    }
}
