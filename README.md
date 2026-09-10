# aviz — Music Visualizer

Real-time, layered audio-reactive music visualizer. Audio comes from local
files or system audio loopback (not direct streaming-service extraction —
see `visualizer-design-spec.md` §3). Visuals are independent modules
composited into a stack that can be reordered, blended, tinted, and
duplicated. See `visualizer-design-spec.md` for full architecture and
`CLAUDE.md` for the working invariants.

## Status

Local playback + feature extraction, waveform/spectrum/radial/particles/
lissajous visuals, the layer compositor (stacking, opacity, blend modes,
tint), layer
duplication + frequency mapping (`freq_band`, `reactivity_source`,
`enabled`), and WASAPI loopback capture with mid-session source switching are
implemented — see "Features" below. The control panel UI doesn't exist yet.
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

- **Local file playback** (WAV/MP3/FLAC via miniaudio) and **WASAPI loopback
  capture** (taps the default output device's mix) both feed the same
  lock-free SPSC ring buffer (overwrite-oldest on overflow, with a
  discontinuity flag for the worker to resync on). Both producers convert
  their native format to a canonical 48kHz/stereo/f32 pipeline format before
  pushing frames, so the ring buffer, worker, and feature extractor are
  built once, against fixed constants, and never rebuilt when the source
  changes. `ProducerHost` is the sole owner of the active producer and the
  only code path that can switch it — always stopping the old producer to
  completion before starting the next, so a source switch can't race the
  next producer's first frames onto the ring buffer.
- **Feature extraction worker thread**: windowed FFT (pffft, 1024/512
  window/hop), mapped into 16 log-scaled bands + 1 global band; per-band
  spectral-flux onset detection with an adaptive rolling threshold; one-pole
  attack/release envelope smoothing on band energy; RMS and peak frequency.
- **Triple-buffered `AudioFeatures` handoff** from the worker thread to the
  render/consumer side — see `CLAUDE.md` invariant #1 for why a plain
  double-buffer isn't sufficient here.
- **Waveform, spectrum bar, radial, and particle visuals**, each rendering
  to its own offscreen texture, cleared to transparent so the compositor can
  stack them. Spectrum/radial bars normalize per band against a
  peak-hold-with-slow-release reference (fast attack, slow release) rather
  than a single shared scale, since band energy magnitude varies by orders
  of magnitude bass-to-presence; both settle to a flat/idle rest pose below
  an RMS floor instead of rendering near-silence noise. Radial maps the same
  per-band bars around a circle instead of left-to-right. Particles is a
  fixed-size pool (no per-frame allocation): `BandOnset`/`GlobalBeat` fire a
  decaying-count burst from center on each onset, `BandAmplitude` spawns
  continuously at a rate proportional to the band's energy/reference ratio;
  spawn direction follows the same per-band angle grid as Radial's bars
  (random full-circle direction for Global), and particles fade out linearly
  over a fixed lifetime.
- **Lissajous** plots the stereo pair directly (left channel on X, right on
  Y) as a connected trace, reusing `WaveformVisual`'s reactivity/onset-pulse
  logic since it's a full-mix (not per-band) visual like Waveform. Idle
  collapses to a single center point rather than trusting near-zero samples,
  matching the other visuals' forced rest pose. `AudioFeatures` carries
  `waveform_l`/`waveform_r` snapshots alongside the existing mono `waveform`
  for this; `FeatureWorker` slides three parallel windows (mono, left,
  right) per hop rather than deriving stereo from the mono mixdown.
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
  `BandAmplitude`). Spectrum draws a single bar for a
  band-scoped layer, positioned/sized as it would sit in the full 16-band
  layout. Waveform still draws the full-mix signal (there's no per-band
  time-domain data) but modulates it: `BandAmplitude`/scoped-band energy
  scales its amplitude, `BandOnset`/`GlobalBeat` trigger a decaying
  line-thickness pulse on that band's onset. Spectrum's bar height always
  tracks its band's continuous energy/reference ratio regardless of mode
  (opacity/tint alpha stays fixed at the layer's normal value throughout);
  `BandOnset`/`GlobalBeat` additionally pulse that bar's RGB toward white on
  a hit, decaying back to its normal tint color afterward (17 independent
  per-band values so switching a layer's `freq_band` never shows another
  band's stale pulse).
- **Per-layer `sensitivity`**: a `[0, 4]`-clamped multiplier (default `1.0`)
  on a layer's reactivity strength, independent of every other layer
  reading the same band. Direct-multiplies the amplitude channel
  (Waveform's amplitude scale, Spectrum's bar-height ratio — both
  uncapped past 1.0, by design) and, pre-clamp, sets the snap-up magnitude
  of an onset-driven pulse (Waveform's line-thickness pulse, Spectrum's
  per-band white-flash pulse).
