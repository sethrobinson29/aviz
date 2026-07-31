// Bootstrap stub: confirms the toolchain and both native dependencies link
// and run, without touching audio hardware or opening a GPU window — those
// come with milestone 1 (see visualizer-design-spec.md §8) and must use the
// no-hardware fallback paths described in TESTING.md, not this entry point.
#include "audio/miniaudio_backend.h"
#include "raylib.h"

#include <cstdio>

int main() {
    std::printf("aviz bootstrap\n");
    std::printf("miniaudio %s\n", audio_backend_version());
    std::printf("raylib %s\n", RAYLIB_VERSION);
    return 0;
}
