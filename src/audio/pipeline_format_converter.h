#pragma once

#include <cstddef>
#include <memory>

class AudioRingBuffer;

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
