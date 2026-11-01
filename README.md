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
one remaining milestone 8 item. **No launch argument selects an audio
source anymore**: `aviz.exe` starts with no active producer at all, and an
audio source (import a local file, or explicitly enable loopback capture)
is chosen from the control panel after launch — see "Features" below.
Build order follows `visualizer-design-spec.md` §8.

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

Requires CMake 3.24+, Visual Studio 2022 or newer with the "Desktop
development with C++" workload, and the WebView2 Runtime (pre-installed on
current Windows 10/11; otherwise grab the Evergreen bootstrapper from
Microsoft — the app currently uses the system-installed runtime, not a
bundled one, see `CLAUDE.md`).

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
ctest --test-dir build -C Release
```

(Substitute `Debug` if that's what you built. The Visual Studio generator is
multi-config, so `-C` is required — without it, ctest silently finds
nothing to run.)

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
- **Fractal** renders the Mandelbrot/Julia set in real time via a GLSL fragment
  shader — the only custom shader in the codebase, since a full-canvas
  per-pixel escape-time computation is far too slow on the CPU at 60fps. An
  orbiting point sweeps through Julia-constant space at a constant rate; the
  scoped band's smoothed amplitude drives zoom and orbit radius directly
  (not the orbit's speed), so the fractal's shape and scale breathe with the
  music while its motion stays steady. See "Per-visual-type panel controls"
  below for the full control set and its one known limitation.
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
  The folder is located from the executable's own path, not the working
  directory, so the same presets appear however the app is launched
  (shortcut, another shell, double-click). Each build configuration has its
  own executable folder and therefore its own `presets/`
  (`build/Debug/presets`, `build/Release/presets`). Paths are handled as
  UTF-8 throughout, so preset and audio files with non-ASCII names work; a
  file in `presets/` whose name is not valid Unicode is left out of the list.
  Every layer carries a stable `layer_id`, assigned on creation and
  preserved across save/load, so a reference to a specific layer survives a
  reorder — loading validates the id set (no duplicates, none invalid)
  before replacing the in-memory stack, rejecting the whole file rather than
  partially applying a malformed one. Every numeric field in a preset is
  clamped to its own valid range on load, whether or not it has a panel
  control (opacity, sensitivity, line/bar thickness, spectrum gap and
  height, particle radius and spawn rate, Fractal smoothing time and
  boundary fade), so a hand-edited preset with out-of-range numbers loads
  with those values pulled to the nearest limit instead of as written. A
  palette override with no stops, or with stops out of ascending order, is
  rejected; stop positions outside 0-1 are clamped. Presets are saved, listed and loaded from the
  control panel (see "Layer management from the panel" below).
- **Palette override** (milestone 7): a per-layer `palette_override`
  gradient (`Layer.palette_override`) that replaces a visual's own flat
  color with a sampled gradient. A layer with an override skips its tint
  multiply, so the gradient shows its true colors at any stack position;
  the layer keeps its assigned tint, and clearing the override brings the
  normal tinted look straight back. Each layer row in the panel has a
  **Palette** dropdown: None, Warm-Cool, or Sunset. A hand-edited preset
  can carry any other gradient, which shows as a read-only **Custom**
  entry until you pick something else. Overrides save and load with
  presets. Wired into all seven visual modules, each
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
  `web/panel/` rather than a `file://` URL), restricted navigation (top-level
  and frame navigations outside the panel's own virtual host are cancelled,
  new-window requests are refused, and any resource request to another
  origin is answered with a blocked response), a page-level Content
  Security Policy, and DevTools disabled in release builds (release builds
  also clear the `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS` environment
  variable before starting WebView2, so it cannot switch remote debugging
  back on). The panel page cannot ask the app to open a file by path: an
  audio file can only be chosen through the native file dialog. WebView2
  callbacks never call `Compositor` (or any window-mode-changing function)
  directly — they push onto a mutex-guarded `PanelDispatchQueue` that the
  render thread drains once per frame and applies via `panel_message_apply`,
  the same path for every message type.
- **Layer management from the panel**: add (choose visual type)/remove/
  duplicate/drag-to-reorder, plus every generic per-layer control (opacity,
  blend mode, palette override, `freq_band`, `reactivity_source`,
  `sensitivity`, `enabled`)
  and preset list/load/save — all reusing the existing tested
  `layer_preset`/`preset_file_io` schema and validation. The keyboard debug
  tool that predated the panel has been removed: the only keys the
  visualizer window still handles are `F11` (fullscreen), `K` (dock) and
  `L` (audio source). Changing an existing layer's visual type is no longer possible;
  remove the layer and add one of the type you want.
  `preset_file_io::is_safe_preset_filename` validates user-typed filenames
  against path traversal, separators, unexpected extensions, drive or
  stream qualifiers (`C:name.json`, `name:stream.json`), other characters
  Windows does not allow in filenames, and reserved device names
  (`NUL.json`, `CON.json`, ...), since the panel is the first place a
  user-typed filename reaches the filesystem.
- **Per-visual-type panel controls (Particles, Flow Field, Lissajous, Radial, and Fractal so far).** A
  generic `set_visual_param` message (`layer_id`, `param_name`, `value` —
  a JSON number or a JSON boolean; a param's setter rejects the wrong type
  rather than coercing it) and
  `visual_param::set` dispatch (`src/compositor/visual_param.h`/`.cpp`) let
  the panel expose a visual module's own params as real sliders, not just
  the generic per-layer fields above — the panel already receives every
  layer's full `params` object on every `layer_stack` push (same
  `layer_preset` JSON used for save/load), so this only adds the write path.
  Particles gets **Spawn Rate** (`spawn_rate_per_frame`, 0–20) and **Draw
  Distance** (`lifetime_frames`, 10–600 frames — particles move at a fixed
  spawn-time velocity, so how long one lives is what determines how far it
  travels before disappearing; there's no separate travel-distance field).
  It also gets **Inner Radius** (`inner_radius_fraction`, 0–1, default 0):
  how far from center particles spawn, as a fraction of half the shorter
  canvas dimension. There is no outer radius for Particles — Draw Distance
  and off-canvas cleanup govern how far they travel.
  All three clamp server-side (`particles_math::clamp_spawn_rate_per_frame`/
  `clamp_lifetime_frames`/`clamp_inner_radius_fraction`) regardless of what
  the slider sends. Flow Field
  gets **Spawn Rate** (`spawn_rate_per_frame`, its own 0–5 range — the pool
  saturates around 2.6 at the default lifetime, so Particles' 0–20 would be
  mostly dead travel), **Trail Life** (`lifetime_frames`, 10–600 — Flow Field
  particles rarely leave the canvas, so lifetime is what caps population and
  persistence), **Turbulence** (`turbulence_frequency`, 0.002–0.05),
  **Evolution Speed** (`evolution_per_frame`, 0–0.05), **Grid Resolution**
  (`grid_cols`, 8–32, rows derived at the default 32:18 ratio), and two real
  boolean checkboxes, **Ripple on Onset** and **Burst on Onset**. The three
  field-shape params are new `FlowFieldParams` fields saved in layer presets
  and required on load, so a preset with a Flow Field layer saved before
  they existed no longer loads. Lissajous gets **Curve Exponent**
  (`compression_exponent`, 0.2–1.0, default 0.5): each raw stereo sample is
  drawn at `sign(x)·|x|^exponent`, so 1.0 is a linear plot (quiet passages
  collapse toward center) and lower values boost quiet samples more (the
  figure stays open). It is a new `LissajousParams` field saved in layer
  presets and required on load, so a preset with a Lissajous layer saved
  before it existed no longer loads. Radial gets **Inner Radius**
  (`inner_radius_fraction`, 0–0.95, default 0.15) and **Outer Radius**
  (`max_radius_fraction`, 0.05–1.0, default 0.9), which together set the
  empty-center size and maximum bar reach. The two must stay at least 0.05
  apart: the slider being dragged stops at that limit and the other one
  never moves, enforced server-side against the layer's current values and
  mirrored in the panel so the thumb stops too. Loading a preset applies
  the same rule (outer clamped first, then inner against it) instead of
  trusting the file. Fractal (the Julia-set shader) gets **Zoom**
  (`base_zoom`, 1.0–4.0, default 1.0 — never below 1.0, where the view
  lands on near-empty regions), **Zoom Reactivity** (`zoom_amplitude_scale`,
  0–2.0, default 0.5 — how much the band's amplitude adds to zoom),
  **Orbit Radius** (`julia_base_radius`, 0.70–0.85, default 0.7885),
  **Radius Reactivity** (`julia_radius_amplitude_scale`, 0–0.2, default
  0.05), **Orbit Speed** (`julia_angular_speed_radians_per_sec`, 0–1.0,
  default 0.15), **Orbit Center X/Y** (`julia_orbit_center_x`/`_y`,
  −0.5–0.5, default 0), a **Clockwise Orbit** checkbox
  (`julia_orbit_clockwise`), and **Color Cycle Speed**
  (`color_cycle_speed_hz`, 0–0.5, default 0.05; 0 holds the color still).
  All ranges are untuned starting values, clamped server-side and again on
  preset load. Orbit speed, direction, and color cycle speed advance an
  accumulated phase each frame, so changing any of them mid-session carries
  on from the current position instead of jumping. The generic per-layer
  `sensitivity` slider now affects Fractal layers too: it scales the band's
  amplitude before that amplitude drives zoom and orbit radius, saturating
  at full amplitude. **Known limitation:** Orbit Radius and Orbit Center
  together decide which Julia sets the orbit passes through, and some
  combinations give a mostly empty layer for part or all of the orbit — no
  error, just little to see; move either control back toward its default.
  Fractal's amplitude smoothing time, boundary fade width, and iteration
  count are not exposed. Other visual modules' params can add their own
  case to `visual_param::set` the same way, without a new message type per
  module.
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
  truth for the current audio source (none/file/loopback), shared by the
  `L` hotkey and the panel's source-switch button — no second switching
  mechanism alongside `ProducerHost::switch_to`.
- **No default audio source at launch.** `aviz.exe` takes no audio-file
  argument anymore and starts with no active `AudioProducer` at all
  (`ProducerHost` supports this "nothing active yet" state as a first-class
  case, not an edge case bolted onto the existing stop-then-start switching
  logic). The panel's **Import File…** button opens a native Windows file
  picker and switches to the chosen file; **Enable Loopback** explicitly
  switches to WASAPI capture. Both go through the same dispatch-queue path
  as every other panel action — the file dialog itself runs on the WebView2
  thread, but only ever enqueues the resulting path for the render thread to
  apply, never calling `ProducerHost`/`AudioSourceController` directly. The
  panel's source-switch button shows "No Source" and is disabled until a
  source is first chosen; it then toggles between the two as before.