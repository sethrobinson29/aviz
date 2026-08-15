# aviz — Music Visualizer

Real-time, layered audio-reactive music visualizer. Audio comes from local
files or system audio loopback (not direct streaming-service extraction —
see `visualizer-design-spec.md` §3). Visuals are independent modules
composited into a stack that can be reordered, blended, tinted, and
duplicated. See `visualizer-design-spec.md` for full architecture and
`CLAUDE.md` for the working invariants.

## Status

Milestone 1 (local playback + feature extraction) is implemented and
verified — see "Features" and "Running" below. Build order follows
`visualizer-design-spec.md` §8. No compositor, visuals, or control panel UI
yet.

## Stack

- C++17 core
- [miniaudio](https://github.com/mackron/miniaudio) — audio I/O, file decoding, and loopback capture
- [pffft](https://github.com/marton78/pffft) — real-valued FFT for feature extraction
- [raylib](https://github.com/raysan5/raylib) — rendering (not yet wired up to any output)
- WebView2 (HTML/CSS/JS) — control panel UI (not yet built)

## Platform

Windows only, for now. Build natively with MSVC + the Visual Studio
generator — not through WSL2. See `CLAUDE.md`'s Target Platform section.

## Building

Requires CMake 3.24+ and Visual Studio 2022 or newer with the "Desktop
development with C++" workload.

```
cmake -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
```

(Substitute your installed VS generator, e.g. `"Visual Studio 17 2022"`, if
different.)

Dependencies (miniaudio, pffft, raylib, GoogleTest/GoogleMock) are fetched
and pinned to specific release tags via CMake FetchContent — no manual
installation needed.

## Running

```
build/Release/aviz.exe <path-to-audio-file>
```

Plays the file through the default output device (WAV, MP3, and FLAC —
whatever miniaudio's built-in decoders cover; no OGG/Vorbis support wired up
yet) while logging live feature-extraction output to the console every
250ms: RMS, peak frequency, global-band energy, and whether an onset fired
in the global band since the last print.

## Testing

```
ctest --test-dir build
```

See `TESTING.md` for what's covered by automated tests versus human review.

Concurrency diagnostic (ThreadSanitizer, WSL2 + Clang, run at milestone
checkpoints): see `TESTING.md`'s Concurrency section for the full recipe, or
run directly from `tsan/`.

## Features

- **Local file playback** (WAV/MP3/FLAC via miniaudio) feeding a
  lock-free SPSC ring buffer (overwrite-oldest on overflow, with a
  discontinuity flag for the worker to resync on).
- **Feature extraction worker thread**: windowed FFT (pffft, 1024/512
  window/hop), mapped into 16 log-scaled bands + 1 global band; per-band
  spectral-flux onset detection with an adaptive rolling threshold; one-pole
  attack/release envelope smoothing on band energy; RMS and peak frequency.
- **Triple-buffered `AudioFeatures` handoff** from the worker thread to the
  render/consumer side — see `CLAUDE.md` invariant #1 for why a plain
  double-buffer isn't sufficient here.
