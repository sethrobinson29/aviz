#pragma once

class AudioRingBuffer;

// Common contract for anything that pushes PCM into the shared ring buffer
// (local file playback, system loopback capture). Downstream code never
// special-cases which implementation is active.
class AudioProducer {
public:
    virtual ~AudioProducer() = default;

    // `ring` must outlive the call to stop(). Converts this producer's
    // native output to audio_config::kPipelineSampleRate/kPipelineChannels
    // before pushing into `ring`.
    virtual void start(AudioRingBuffer& ring) = 0;

    // Must not return until no further call into this producer's data
    // callback can occur -- callers may assume it is safe to start a
    // different producer against the same ring buffer immediately after
    // stop() returns.
    virtual void stop() = 0;

    // Native format, for status/debug display only.
    virtual unsigned native_channels() const = 0;
    virtual unsigned native_sample_rate() const = 0;
};
