#pragma once

#include <cstddef>
#include <memory>

class AudioRingBuffer;

// Converts interleaved f32 PCM from a producer's native channel
// count/sample rate to audio_config::kPipelineChannels/kPipelineSampleRate,
// then pushes the converted frames into a ring buffer. Shared by every
// AudioProducer implementation so the canonical-format conversion logic
// exists once. No per-call allocation once constructed.
class PipelineFormatConverter {
public:
    PipelineFormatConverter(unsigned channels_in, unsigned sample_rate_in);
    ~PipelineFormatConverter();

    PipelineFormatConverter(const PipelineFormatConverter&) = delete;
    PipelineFormatConverter& operator=(const PipelineFormatConverter&) = delete;

    void convert_and_push(const float* interleaved_in, std::size_t frame_count_in,
                           AudioRingBuffer& ring);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
