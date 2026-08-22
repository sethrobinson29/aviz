# aviz — Music Visualizer

Real-time, layered audio-reactive music visualizer. Audio comes from local
files or system audio loopback (not direct streaming-service extraction —
see `visualizer-design-spec.md` §3). Visuals are independent modules
composited into a stack that can be reordered, blended, tinted, and
duplicated. See `visualizer-design-spec.md` for full architecture and
`CLAUDE.md` for the working invariants.

## Status

Local playback + feature extraction, waveform/spectrum visuals, the layer
compositor (stacking, opacity, blend modes, tint), and layer duplication +
frequency mapping (`freq_band`, `reactivity_source`, `enabled`) are
implemented — see "Features" below. The control panel UI doesn't exist yet,
so the layer stack is driven by a temporary keyboard-controlled debug tool.
Build order follows `visualizer-design-spec.md` §8.

## Stack

- C++17 core
- [miniaudio](https://github.com/mackron/miniaudio) — audio I/O, file decoding, and loopback capture
- [pffft](https://github.com/marton78/pffft) — real-valued FFT for feature extraction
- [raylib](https://github.com/raysan5/raylib) — rendering
- WebView2 (HTML/CSS/JS) — control panel UI (planned)

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
- **Waveform and spectrum bar visuals**, each rendering to its own
  offscreen texture, cleared to transparent so the compositor can stack
  them. Spectrum bars normalize per band against a peak-hold-with-slow-release
  reference (fast attack, slow release) rather than a single shared scale,
  since band energy magnitude varies by orders of magnitude bass-to-presence.
  Both settle to a flat/idle rest pose below an RMS floor instead of
  rendering near-silence noise.
- **Layer compositor**: add/remove/reorder layers, each with independent
  opacity, blend mode (normal, additive, multiply, screen), and tint color.
  Layers composite in stack order, bottom to top, into a single output
  texture. Waveform/spectrum both draw in white by default so the
  compositor's multiply-based tint isn't fighting a pre-colored base.
  Duplicating a layer clones its visual and copies all of its other fields
  (opacity, blend mode, tint, `freq_band`, `reactivity_source`, `enabled`).
  Disabled layers are skipped entirely — no offscreen render, no
  compositing pass.
- **Per-layer `freq_band` and `reactivity_source`**: every layer can be
  scoped to a specific band (0–15) or Global, and tagged with what
  audio-derived value it's meant to react to (`GlobalBeat`, `BandOnset`,
  `BandAmplitude`, `PeakFreq`). Spectrum draws a single bar for a
  band-scoped layer, positioned/sized as it would sit in the full 16-band
  layout. Waveform still draws the full-mix signal (there's no per-band
  time-domain data) but modulates it: `BandAmplitude`/scoped-band energy
  scales its amplitude, `BandOnset`/`GlobalBeat` trigger a decaying
  line-thickness pulse on that band's onset. `PeakFreq` isn't wired to
  either visual yet.
