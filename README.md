# aviz — Music Visualizer

Real-time, layered audio-reactive music visualizer. Audio comes from local
files or system audio loopback (not direct streaming-service extraction —
see `visualizer-design-spec.md` §3). Visuals are independent modules
composited into a stack that can be reordered, blended, tinted, and
duplicated. See `visualizer-design-spec.md` for full architecture and
`CLAUDE.md` for the working invariants.

## Status

Local playback + feature extraction, waveform/spectrum/radial/particles/
lissajous/flow field/fractal visuals, the layer compositor (stacking,
opacity, blend modes, tint, palette override), layer duplication + frequency
mapping (`freq_band`, `reactivity_source`, `enabled`), layer preset
save/load, and WASAPI loopback capture with mid-session source switching are
implemented — see "Features" below. The WebView2 control panel is now a real
layer-management UI (add/remove/duplicate/drag-reorder, generic per-layer
controls, preset save/load) with security defaults, docked/detached window
management, and synced fullscreen/dock/source-switch controls (panel UI,
hotkey, and hotkeys-from-either-window all agree) — theme selection is the
one remaining milestone 8 item. Build order follows
`visualizer-design-spec.md` §8.

## Stack

- C++17 core
- [miniaudio](https://github.com/mackron/miniaudio) — audio I/O, file decoding, and loopback capture
- [pffft](https://github.com/marton78/pffft) — real-valued FFT for feature extraction
- [raylib](https://github.com/raysan5/raylib) — rendering
- WebView2 (HTML/CSS/JS) — control panel UI

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
- **Flow field** reuses Particles' pool/spawn/lifetime/cleanup machinery,
  changing only how velocity is determined: each particle steers toward a
  direction sampled from a fixed-resolution grid of hand-rolled 2D value
  noise, regenerated every frame and animated by a persistent phase.
  Turbulence (noise frequency) and evolution speed (phase advance) both
  scale continuously with the scoped band's amplitude, independent of
  onset events. An onset additionally (a) bursts new particles via
  Particles' existing onset-burst mechanism and (b) spawns an expanding
  ring impulse that pushes already-in-flight particles outward as it
  passes — two independent, simultaneously-firing effects, not a mode
  switch. Particles render as short velocity-aligned streaks rather than
  dots.
- **Layer compositor**: add/remove/reorder layers, each with independent
  opacity, blend mode (normal, additive, multiply, screen), and tint color.
  Layers composite in stack order, bottom to top, into a single output
  texture. Waveform/spectrum both draw in white by default so the
  compositor's multiply-based tint isn't fighting a pre-colored base.
  Duplicating a layer clones its visual and copies all of its other fields
  (opacity, blend mode, tint, `freq_band`, `reactivity_source`, `enabled`).
  Disabled layers are skipped entirely — no offscreen render, no
  compositing pass.
- **Layer preset save/load**: the full layer stack (visual type, opacity,
  blend mode, tint, palette override, `freq_band`, `reactivity_source`,
  `sensitivity`, `enabled`) serializes to/from a versioned JSON schema
  (`layer_preset.h`/`.cpp`) and round-trips through `presets/` (a
  runtime-only folder next to the executable, not checked into the repo).
  Every layer carries a stable `layer_id`, assigned on creation and
  preserved across save/load, so a reference to a specific layer survives a
  reorder — loading validates the id set (no duplicates, none invalid)
  before replacing the in-memory stack, rejecting the whole file rather than
  partially applying a malformed one. The debug tool's `S`/`O` keys exercise
  this today (save/load a fixed debug filename); real panel-driven preset
  management is still milestone 8 work.
- **Palette override** (milestone 7): a per-layer `palette_override`
  gradient (`Layer.palette_override`) that replaces a visual's own flat
  color with a sampled gradient, layered under the existing tint multiply
  rather than replacing it. Wired into all seven visual modules, each
  sampling by its own natural axis: Spectrum/Radial by each bar's frequency
  position, Waveform/Lissajous by position along the drawn line,
  Particles/Flow Field by each particle's life fraction (a color-over-life
  gradient, composed with the existing lifetime alpha fade), and Fractal by
  escape-depth position via a small GPU gradient texture built from the
  palette and sampled in-shader (offset by the existing slow hue-cycle
  phase, so the established "static or slow time-based color cycling"
  character carries over instead of being replaced). Every module falls
  back to its existing flat/rainbow color logic when no override is set.
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
- **Control panel window**: a `UIHost` interface (`src/ui_host/ui_host.h`)
  with a WebView2 implementation, opening alongside the raylib visualizer
  window with no shared-surface compositing, on its own dedicated thread.
  Security defaults ship with it, not as a follow-up: scoped local file
  access (served via `SetVirtualHostNameToFolderMapping` against
  `web/panel/` rather than a `file://` URL), restricted navigation (anything
  outside the panel's own virtual host is cancelled), a page-level Content
  Security Policy, and DevTools disabled in release builds. WebView2
  callbacks never call `Compositor` (or any window-mode-changing function)
  directly — they push onto a mutex-guarded `PanelDispatchQueue` that the
  render thread drains once per frame and applies via `panel_message_apply`,
  the same path for every message type.
- **Layer management from the panel**: add (choose visual type)/remove/
  duplicate/drag-to-reorder, plus every generic per-layer control (opacity,
  blend mode, `freq_band`, `reactivity_source`, `sensitivity`, `enabled`)
  and preset list/load/save — all reusing the existing tested
  `layer_preset`/`preset_file_io` schema and validation, now exposed through
  real UI instead of the debug tool's fixed-filename `S`/`O` keys.
  `preset_file_io::is_safe_preset_filename` validates user-typed filenames
  against path traversal, separators, and unexpected extensions, since the
  panel is the first place a user-typed filename reaches the filesystem.
- **Docked/detached window layout**: at launch (and whenever redocking) the
  visualizer and panel windows are positioned as one adjacent, centered
  block (`window_layout_config.h`'s pure `compute_docked_layout`); detaching
  is a pure state flip that stops enforcing that layout, for the
  panel-on-one-display / visualizer-fullscreen-on-another workflow.
  Dragging either window while nominally docked is treated as an explicit
  detach (caught via `WM_EXITSIZEMOVE`, which only fires for genuine
  interactive drags, never the app's own programmatic repositioning) rather
  than letting them silently drift out of sync.
- **Fullscreen, dock, and audio source are synced three ways**: the panel's
  checkboxes/button, the existing hotkeys (`F11`/`K`/`L`), and — new — those
  same hotkeys now also work while the panel window has focus (a JS
  `keydown` listener re-sends the identical panel message the UI control
  would send, no new native surface). Fullscreen uses raylib's
  `ToggleBorderlessWindowed()`, not `ToggleFullscreen()`'s exclusive
  display-mode switch, specifically to avoid the signal
  renegotiation/mode-switch artifacts some displays show under real
  exclusive fullscreen. `AudioSourceController` is the single source of
  truth for file/loopback state, shared by the `L` hotkey and the panel's
  source-switch button — no second switching mechanism alongside
  `ProducerHost::switch_to`.
