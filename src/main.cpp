// Milestone 1 entry point: local file playback + feature extraction (see
// visualizer-design-spec.md §8). No visuals or GPU window yet — that's
// milestone 2 — so correctness here is checked by listening to real
// playback while watching the logged AudioFeatures summary, per
// TESTING.md's audio-correctness section (human review, not an agent
// claiming clean output means it works).
#include "audio/audio_config.h"
#include "audio/audio_features.h"
#include "audio/feature_worker.h"
#include "audio/file_producer.h"
#include "audio/ring_buffer.h"

#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <audio-file>\n", argv[0]);
        return 1;
    }

    try {
        FileProducer producer(argv[1]);
        AudioRingBuffer ring(audio_config::kRingBufferCapacityFrames, producer.channels());
        AudioFeaturesSnapshot snapshot;
        FeatureWorker worker(ring, snapshot, producer.sample_rate());

        std::printf("playing %s (%u ch, %u Hz)\n", argv[1], producer.channels(),
                    producer.sample_rate());

        worker.start();
        producer.start(ring);

        // AudioFeatures::band_onset is instantaneous per-hop truth (~11ms
        // wide) — sampling it once per 250ms print would mostly miss real
        // onsets rather than reflect detection actually failing. Polling
        // much faster than the hop period and latching "fired since last
        // print" turns this into an actual verification signal instead of
        // a sampling lottery. This latching is display-only, confined to
        // main.cpp: AudioFeatures itself stays instantaneous, since a
        // future visual consumer may need the exact-hop truth, not a
        // smeared-over-250ms one (CLAUDE.md invariant #2).
        constexpr auto kPollInterval = std::chrono::milliseconds(2);
        constexpr auto kPrintInterval = std::chrono::milliseconds(250);

        bool onset_since_last_print = false;
        auto next_print = std::chrono::steady_clock::now() + kPrintInterval;

        while (!producer.is_finished()) {
            std::this_thread::sleep_for(kPollInterval);
            const AudioFeatures& f = snapshot.current();
            onset_since_last_print |= f.band_onset[audio_config::kGlobalBandIndex];

            if (std::chrono::steady_clock::now() >= next_print) {
                std::printf("rms=%6.3f peak=%7.1fHz global=%6.3f%s\n", f.rms, f.peak_frequency_hz,
                            f.band_energy[audio_config::kGlobalBandIndex],
                            onset_since_last_print ? " [onset]" : "");
                onset_since_last_print = false;
                next_print += kPrintInterval;
            }
        }

        producer.stop();
        worker.stop();
        std::printf("done\n");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
