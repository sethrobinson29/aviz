# Music Visualizer — Design Spec

## 1. Goals

- Real-time audio-reactive visualizer with a layered, compositable system.
- Support both local audio files and "any source" playback (browser tabs, streaming apps) via system audio loopback rather than fragile per-service integrations.
- Users can stack multiple visual layers, each independently configurable and independently audio-reactive.

## 2. Tech Stack

| Concern | Choice | Why |
|---|---|---|
| Language (core) | C++17 | Agent-friendly, single language for audio/visual core, no FFI boundaries within it |
| Audio I/O + loopback | miniaudio | Single-header, wraps WASAPI / Core Audio / PulseAudio-PipeWire loopback behind one API |
| Rendering | raylib | Simple immediate-mode graphics, easy to target for waveform/spectrum/particle visuals |
| FFT | pffft (`marton78/pffft`, v1.1.0) | Real-valued FFT support + SIMD (SSE) optimization on the hot audio-processing path; float precision, SSE only — see `STRUCTURE.md`'s dependency policy for full rationale |
| Control panel UI | WebView2 (HTML/CSS/JS) | Native Windows webview embed; needed to hit real Y2K/Frutiger-Aero visual fidelity (gradients, glass/blur, chrome) — see §7 |

Rejected: Rust + cpal — loopback/capture support is thinner and less battle-tested than miniaudio; would likely require dropping to raw platform APIs anyway.

Note: the control panel UI is a deliberate exception to the "one language, no FFI" principle for the audio/visual core — see §7 for the rationale and the interface boundary (`UIHost`) that contains that exception.

## 3. Audio Input

Two producers feed the same downstream pipeline:

1. **Local file playback** — decode via miniaudio, fixed known sample rate.
2. **System loopback capture** — platform-specific, variable sample rate/buffer size:
   - Windows: WASAPI loopback mode
   - macOS: Core Audio process taps (macOS 14.4+) or BlackHole virtual device fallback; requires user consent (similar to screen-recording permission)
   - Linux: PipeWire / PulseAudio monitor source

Both producers push raw PCM frames into a shared ring buffer via a common interface (`push_frames(samples, count, channels, sample_rate)`), so the rest of the pipeline doesn't know or care which source it came from.

**Design implications:**
- FFT windowing must tolerate variable sample rates/buffer sizes (loopback), not assume a fixed rate like local files provide.
- Loopback has a frame or two of inherent latency — beat detection and visual response should be designed with that slack in mind, since the app is a passive observer of someone else's playback pipeline.
- Explicitly out of scope: direct YouTube/Bandcamp stream extraction. YouTube's player is a sandboxed iframe with no exposed audio stream; Bandcamp's CORS/audio-analysis support is inconsistent and scraping stream URLs isn't an officially supported use. Loopback capture sidesteps both problems and works with any source without touching either site's ToS.

**Ring buffer overflow policy: overwrite-oldest.** If the feature-extraction worker falls behind the audio callback, new frames overwrite the oldest unread frames rather than being rejected. This bounds latency so visuals track live audio rather than drifting further behind under sustained load, which matters more for perceived responsiveness in a real-time visualizer than strict analytical completeness (a reject-new policy would guarantee no frames are skipped, at the cost of unbounded latency growth if the worker can't keep up). Two known tradeoffs of this choice, both accepted:
- A skipped chunk containing a transient means that transient is never seen by onset detection — a possible missed beat under sustained worker overload, rather than a delayed one.
- When the worker's read pointer jumps forward past overwritten frames, the next analysis window is no longer temporally contiguous with the last one processed. Since onset detection is spectral-flux-based (frame-to-frame energy delta), a discontinuity like this can look like a transient and fire a spurious onset. Mitigation: the producer sets a discontinuity flag when an overwrite occurs, so the worker resets its onset-detection history on the next window instead of comparing across the gap as if it were contiguous.

**Worker signaling: fixed-interval polling.** The feature-extraction worker polls for new PCM on a fixed interval rather than being signaled via a condition variable on push. The interval is chosen based on the smallest expected FFT window/hop size (§4), so the worst-case added latency from polling is bounded and known rather than incidental — not an arbitrary round number.

## 4. Feature Extraction (shared, computed once per frame)

All visuals consume the same computed feature set — never raw samples directly.

- **Banded spectrum**: FFT output binned into ~8–32 log-scaled bands (sub-bass through presence/air), plus presets like bass/low-mid/high-mid/treble or a simpler bass/mid/treble/full, with raw Hz ranges editable underneath.

  **Concrete layout: 16 log-scaled bands + 1 global band = 17 total, fixed-size (`std::array`, no per-frame allocation).** 16 was chosen as a middle of the spec's 8–32 range, divides evenly into 4-band preset groupings (bass/low-mid/high-mid/treble) with no remainder, and is enough granularity for band-mapped layers to feel distinct without carrying 32 bands' worth of onset-detection history state for marginal benefit. The global band sits at the **last index** (`kGlobalBandIndex = kNumBands = 16`), not prepended or interleaved — this keeps the log-scaled bands at indices `0..15` contiguous and trivially iterable (`for (i = 0; i < kNumBands; i++)`) for anything sweeping "all real frequency bands" (spectrum visual, preset grouping, UI band list), since global is an aggregate, not a point on the log-frequency sweep, and gets its own named constant instead. `kTotalBands = kNumBands + 1 = 17`.

  **Band-edge-to-bin mapping: running cursor, not independent per-band rounding — fixes a real bug, not a hypothetical.** Found via visual testing (unpredictable smearing on the low end) rather than by the original unit tests: rounding each band's edges to FFT bins independently let adjacent low-frequency bands round to the *same* bin at the ~46.9 Hz/bin resolution (§4's window/hop decision), since log-spaced band edges are closer together than the linear bin resolution down there. Two bands both reading the same bin isn't just coarse data, it's aliasing — data bleeding across a band boundary that's supposed to be a clean separation. Fixed with a running bin cursor: each band's start bin is forced to be the previous band's end (`bin_hi = min(max(bin_for(edge_hi), bin_cursor + 1), kNumFftBins)`), guaranteeing a contiguous, non-overlapping partition of the spectrum with every band getting at least one bin. This is watertight for the current constants (`kNumBands = 16` vs. `kNumFftBins = 513`, nowhere close to the margin where the trailing clamp could produce a zero-width band) but isn't a universal invariant independent of them — a debug-build assert on both ends of the guarantee (every band's `bin_end > bin_begin`, and the final band's `bin_end` doesn't exceed `kNumFftBins`) makes this self-checking rather than something that has to be re-derived by hand if the band count or FFT size ever changes (band count changing at all requires discussion per `CLAUDE.md` invariant #3, but the assert costs nothing in release builds and this is the second bug found at this exact low-end coarse-resolution boundary — worth the cheap defense).
- **Per-band onset/beat detection**: spectral-flux-style (frame-to-frame energy increase, peak-picked above a rolling threshold), computed independently per band. A kick-drum transient in the sub-bass band and a hi-hat/snare transient in the 2–5kHz band trigger differently, which is what makes band-mapped layers feel distinct rather than just synced pulses.

  **Concrete threshold: `threshold[n] = max(mean(flux, trailing window) + k·stddev(flux, trailing window), absolute_floor)`.** Trailing window ~92 frames (~1 sec at hop 512/48kHz), `k` ~1.5–2.0, refractory period ~50–100ms minimum inter-onset time to prevent a single transient's decay from double-firing — all tunable constants per the requirement below. Maintained incrementally (running sum/sum-of-squares over a circular buffer, O(1) per update) rather than via a true rolling median, since this runs per-band (17 bands) every hop and the performance-priority guideline (`CLAUDE.md`) favors it; median's extra robustness to outliers isn't needed here.

  **`absolute_floor = 0.1f`, tunable — added to fix a real robustness bug, not a hypothetical.** With an all-zero flux history (true silence, no noise floor), the adaptive term alone (`mean + k·stddev`) collapses to exactly 0, so the *first* nonzero flux value in that band — however tiny — fired an onset regardless of whether it was a real transient. This caused every band to fire simultaneously right at a track's true first onset, since the same collapse happened everywhere at once. A relative-only threshold can't self-correct for this: the floor has to be an absolute value independent of the trailing window's own statistics. Found by real listening + live debug output, not by an existing test — worth remembering this class of bug (threshold math degenerate at a boundary condition) isn't something the original synthetic fixtures happened to cover.
- **Global beat detection**: implemented as a degenerate case of the same per-band computation, with the band scoped to full-spectrum energy — not a separate detection system. Gives layers a consistent "master clock" to lock onto without requiring a second algorithm.
- **RMS/volume** and **peak frequency** as simple scalar features.

Struct shape (conceptual): a per-frame snapshot containing raw recent waveform samples, an array of band energies + onsets (including the full-spectrum/global band), RMS, and peak frequency — handed to every active layer each frame.

**Publishing mechanism: triple buffering, not a 2-slot double-buffer.** Three preallocated `AudioFeatures` instances: the worker thread exclusively owns a private back slot (writes new results into it), the render thread exclusively owns a private front slot (reads from it), and a third shared middle slot changes ownership between them. This took two structural iterations to get right, not one — see `TESTING.md`'s track record note for the full sequence. The shipped mechanism: the slot index and a new-data flag are packed into a single atomic word (not two separate atomics), and the consumer side claims the shared slot via a **compare-exchange retry loop**, not a blind atomic exchange — a blind exchange on the consumer side is exactly what caused the second regression, since it let the consumer briefly hold a slot the producer still believed it owned. This was not the first design — a simpler 2-slot double-buffer with an atomic index/pointer was tried first and found to have a genuine data race: since the worker must never block (`CLAUDE.md` invariant #1), nothing bounds how many times it can publish while the render thread is still mid-read (a multi-statement read, e.g. `f.rms` then `f.band_energy[...]`) of the slot it's holding a reference to, so the worker can wrap back into that exact slot while it's still being read. Triple buffering with the packed-word/CAS handoff removes this structurally: ownership transfer is atomic and contested correctly, so a reader either has exclusive access to a complete, consistent front slot or it doesn't — there's no partial-overwrite window. Worst case under adversarial timing is the render thread reusing one stale frame for an extra cycle, which is benign (same tolerate-staleness-over-corruption spirit as the ring buffer's discontinuity flag), not a torn/inconsistent read.

**FFT window/hop size: 1024/512.** At a typical 48kHz sample rate this gives ~21.3ms windows, ~46.9 Hz/bin frequency resolution, and an update rate (~93/sec) comfortably above display refresh (60fps) — no visible stepping in feature values across render frames. Chosen as a middle ground between 512/256 (finer timing, coarser frequency resolution) and 2048/1024 (finer frequency resolution, more added latency, update rate close enough to 60fps to risk visible stepping).

**Window size, hop size, band boundaries, and envelope attack/release constants must all be tunable** — named constants or a small config struct in the audio/feature-extraction code, not literals scattered through the FFT/banding logic. This project is expected to need by-ear/by-eye tuning once it's actually running, independent of any future shipping ambitions; hardcoding these values would mean a recompile for every experiment.

**Envelope smoothing (resolves the §9 gap): asymmetric one-pole IIR, separate attack/release coefficients.** `env[n] = coeff·env[n-1] + (1-coeff)·value`, where `coeff` switches between an attack coefficient when the input is rising and a release coefficient when falling, each derived from a target time constant via `coeff = exp(-hop_time / tau)`. Starting values: attack `tau` ~10–20ms (responsive to a transient), release `tau` ~150–300ms (avoids flicker, gives a felt decay rather than a hard cutoff). Applied to `band_energy` before it's ever handed to a visual layer. Both time constants are tunable per the requirement above.

**No-signal idle state (resolves the §9 gap): reuses the envelope, no separate mechanism.** When audio pauses or a source goes silent, visuals fade to a flat/rest appearance rather than snapping — but this isn't new logic. Waveform/spectrum display must gate on the **already-smoothed envelope value**, not raw RMS: RMS reflects the instantaneous signal and would snap to flat the moment input stops, while the smoothed value naturally decays over the existing ~150–300ms release curve, which is exactly the fade behavior wanted. A dedicated idle cross-fade was considered and rejected — it would either duplicate the envelope's release curve (redundant) or paper over the envelope not actually driving the display (a bug to fix, not a reason for a second smoothing pass). The only new piece is an **epsilon floor** (tunable constant, e.g. `kIdleFloorEpsilon`): a one-pole exponential decay approaches zero asymptotically but never reaches it exactly, so once the smoothed value drops below the floor, snap to exactly flat/zero-height rather than rendering an imperceptibly-still-decaying curve forever. This is shared across all visuals reading `AudioFeatures` — no per-visual-type idle logic needed.

**Per-band AGC reference (normalizes `band_energy` for bar-style display; found via visual testing, four real bugs deep):** spectrum-style visuals need each band's energy normalized against "what's typical for this band" to produce a sensible bar height. A single global reference constant was tried first and rejected — see below — in favor of a per-band adaptive peak-hold reference, a second, independent one-pole filter per band alongside the existing fast display envelope.

*Design, as currently settled:*
- **Two independent filters per band, not one doing double duty:** the existing fast attack/slow release display envelope (above) drives what's shown; a separate, slower AGC reference tracks a peak-hold ceiling used only for normalization. Mixing the two into one filter was explicitly avoided.
- **Fed from `smoothed_energy_` (the same value being displayed), not raw energy.** This is load-bearing, not a style preference — see bug 2 below.
- **Asymmetric peak-hold shape: instant-snap rise, slow decay.** Rise is an unconditional `reference = max(reference, current_value)` each hop — no blending, no tau — so a genuine new peak is captured immediately regardless of how briefly the signal is above the previous reference. Decay is a slow one-pole release, giving the reference a "ceiling that holds" character rather than one that just chases the current level. (Standard, established design for peak-hold meters generally, not novel to this project.)
- **Ratio computed only after the current hop's reference update**, not against the previous hop's stale value — see bug 4 below for why this matters even with instant-snap rise.
- **Seeded with a sensible default at track start / source change, not zero** — prevents both a track that jumps straight into loud material and one that builds up gradually from showing artificially inflated bars while the reference ramps up from nothing.
- Reference decay tau and the seed value are tunable constants per the requirement above; exact tuned values should be confirmed against real material and recorded here once finalized (not yet captured verbatim in this writeup).

*Bugs found and fixed, in order (each confirmed with traced per-hop data, not accepted on plausibility):*
1. **Single global reference collapsed treble variation.** A single fixed constant, calibrated off whichever band naturally hit it (almost certainly bass/low-mid, since that's where music energy concentrates), meant every other band was measured against a scale unrelated to its own typical range — treble bands couldn't show meaningful variation even at their own effective local maximum. Some left/mid skew is real and expected (physics of where energy concentrates); this bug suppressed the top end well beyond that real skew. Static per-band constants were considered as a fix and rejected in favor of adaptive, since the goal is working well across arbitrary audio rather than material tuned to a specific calibration set.
2. **AGC fed from raw energy while the display used smoothed energy — a numerator/denominator mismatch.** The fast-attack/slow-release display envelope rides higher than a plain average (spends more time near recent peaks), so dividing a peak-biased numerator by an unbiased denominator systematically inflated the ratio (explained excess clipping) and caused large relative swings near silence, where both signals collapse toward the same tiny noise floor. Fix: feed the AGC from `smoothed_energy_`, the same quantity being displayed — numerator and denominator became the same signal at two timescales, self-consistent by construction.
3. **A symmetric one-pole reference converges toward the current level rather than providing headroom.** During a sustained loud passage, a plain trailing average settles close to the signal it's normalizing, pinning the ratio near 1 regardless of content (observed as a flat 60–90%+ reading regardless of loudness). Fix: asymmetric rise/decay (peak-hold shape, as in the settled design above) — this is what let normal passages read as a modest fraction (~10–45%, tracking real loudness) instead of pinned near the ceiling.
4. **A blended (tau-based) rise, even after moving to asymmetric rise/decay, was still not fast enough for bursty/gappy content.** The rise branch of a one-pole filter only integrates while the input exceeds the current reference — for peaky, gappy material (a choppy intro), the signal spends very little time actually above the reference at any instant, so the *effective* catch-up rate was far slower than the nominal tau suggested (observed: reference still visibly climbing after 19+ seconds on one track, never settling). This is why classic peak-hold meters use near-instant attack rather than any blended rise, however fast — fixed by switching to true instant-snap (`reference = max(reference, value)`, no tau at all) rather than further tightening a rise tau, which would only have shrunk the gap, not closed it.
5. **A one-hop read/update ordering bug remained even after instant-snap rise.** If a hop's ratio is computed against the *previous* hop's reference, before the reference is updated to reflect the current hop's own energy, a single-hop overshoot spike is structurally guaranteed on the exact transient hop — true regardless of how fast the rise mechanism is, since it's a staleness bug, not a timing-constant bug. Traced peak ratio of 7.78 on one event, worse for bands that had been dormant (larger first climb needed) — this was the actual source of the "smooths out over the course of a song" perception, since it's each band's reference needing a big first catch-up then staying primed, not global system settling. Fixed by ensuring the reference is updated before the ratio is computed within the same hop. Verified after the fix: max ratio exactly 1.000 across every window checked, zero overshoot anywhere — confirmed closed, not just reduced.

*Ruled out, with proof rather than assumption:* after all five fixes, remaining perceived visual "jitter" was checked against the possibility of further data-pipeline instability and ruled out mathematically, not just empirically — a large (>0.3) single-hop swing can only be produced by a genuine attack event, since neither mechanism capable of producing a fast *fall* (reference decay, ~0.03%/hop; envelope release, ~5%/hop) can reach that magnitude in one hop. 100% of large swings in the checked data were rises, confirming the data pipeline is correct and the remaining visible jitter is the throwaway debug bar-chart rendering genuinely fast, legitimate onset-driven data with no frame interpolation (~93Hz feature updates against 60fps display) — a renderer concern, not a data concern, and out of scope for this pass since that rendering code is disposable ahead of the real spectrum visual module (§6) being built against the compositor (§5).

## 5. Layer & Compositor System

**Rendering model:** each active layer renders to its own offscreen texture (framebuffer). The compositor then stacks textures in order — this is what makes arbitrary mixing possible, since layers never fight over one shared canvas. **Every offscreen texture must be cleared to fully transparent (alpha 0), not opaque black, before a visual module draws into it.** An opaque-black background composites incorrectly under every blend mode except normal — additive, multiply, and screen all treat background transparency as meaningful, and an opaque background silently produces wrong output rather than an obvious failure. This is a requirement for every visual module (§6), not something to fix per-module after the fact — found as a real bug during milestone 3 when milestone 2's waveform/spectrum modules (built before the compositor existed to expose the problem) needed correcting.

**Layer data model (conceptual):**

```
Layer {
  visual_type: enum (waveform, spectrum, particles, fractal, flow_field, geometric, ...)
  visual_params: struct          // per-type settings
  opacity: float
  blend_mode: enum (normal, additive, multiply, screen, ...)
  tint_color: optional
  palette_override: optional
  reactivity_source: enum (global_beat, band_onset, band_amplitude, peak_freq)
  freq_range: optional { min_hz, max_hz }   // used only for band_* sources
  order_index: int               // position in the stack; also drag-to-reorder
}
```

**Layer stack operations:** add, remove, reorder (drag-to-reorder, same interaction as Photoshop/Premiere layer panels), duplicate. Duplication is a first-class operation — instancing the same visual type multiple times (e.g., three particle layers each mapped to a different frequency band) is an expected, cheap way to build complex visuals without new visual code.

**Color customization**, two layers of control:
- **Palette override** — each visual type has its own internal color logic (e.g., spectrum bars colored by frequency); accepts a custom palette/gradient input.
- **Post-tint** — a universal, cheaper compositor-stage pass (hue shift / multiply by tint color) applied after rendering, giving "recolor anything" without touching each visual module's internals. Ship this first; palette overrides per visual type are a deeper v2 layer on top.

## 6. Visual Modules

Starting set, roughly in build order:

1. **Waveform** — raw amplitude over time (simplest, validates the audio pipeline first)
2. **Spectrum/bars** — FFT magnitude per band, classic EQ look
3. **Radial/circular** — spectrum or waveform mapped around a circle
4. **Lissajous** — stereo L/R channels plotted against each other
5. **Particle systems** — spawn/velocity/color driven by a band's amplitude or onset
6. **Plasma/flow fields** — noise fields perturbed by audio
7. **Fractals** — zoom/rotation/color cycling driven by audio
8. **Geometric/abstract** — rotating polygons, tunnels

Each module implements a common interface: given `AudioFeatures` (filtered to its assigned band, if applicable) and its own `visual_params`, render into its offscreen texture. Per §5's rendering model, that texture must be cleared to transparent before drawing — not opaque black — so the compositor's blend modes work correctly regardless of what's stacked above or below.

## 7. UI Architecture

**Terminology note:** this section covers two distinct concepts that both got called "preset" loosely elsewhere in discussion — keep them separate. A **layer preset** is a saved layer-stack configuration (which visual layers, their params, order) that the user creates and recalls, potentially many of them. A **theme** is one of a small, fixed set of built-in UI skins (Y2K, Frutiger Aero, etc.) selectable from the control panel — not user-created, not open-ended. They're unrelated data: don't conflate their storage, their UI controls, or their persistence format.

**Two separate native windows**, not one process trying to share a GPU compositing tree:

1. **Control panel window** — WebView2-hosted HTML/CSS/JS. Owns the layer stack management UI: add/remove/duplicate/reorder layers, per-layer opacity/blend mode/tint/freq mapping, layer preset save/load, and theme selection. WebView2 was chosen over hand-rolling widgets in raylib specifically to hit real Y2K/Frutiger-Aero visual fidelity (gradients, glass/blur, chrome) — see rejected alternatives below.
2. **Visualizer window** — raylib, renders the composited layer stack. Supports a fullscreen mode with minimal chrome: a small, subtle on-screen button to exit fullscreen, in addition to a hotkey.

**No shared-surface compositing.** The two windows don't attempt to render through each other (e.g., control panel glass showing the live visualizer behind it) — each renders against its own background. This avoids needing WebView2's Composition Hosting API / DirectComposition interop with raylib's OpenGL context, which would otherwise be required to have one surface show through the other.

**Fullscreen behavior:**
- Entering fullscreen is triggered either by a hotkey or a clickable button in the control panel UI.
- On entering fullscreen, the control panel window closes/hides rather than staying open-but-covered.
- Exiting fullscreen is available via the same hotkey or the subtle on-screen button in the visualizer window itself.

**Theming:** a fixed set of built-in themes (data-driven — palette, gradients, chrome/glass treatment per theme), selectable from the control panel, not an open-ended user skin/modding system. Closer to "pick a WinAmp skin from a list" than "build your own skin." Not to be confused with layer presets (above) — themes affect only the panel's own appearance, not the layer stack.

**Platform-facing interfaces, both designed swappable even though only Windows is implemented:**
- Audio: the existing `push_frames` producer interface (§3) — implementations are per-OS (WASAPI now; Core Audio/PipeWire documented, not built).
- UI: a `UIHost` interface abstracting the platform webview embed. WebView2 implements it now; a future Linux/macOS implementation (WebKitGTK, WKWebView) would satisfy the same interface without changing the state-bridge protocol or app-side logic. This is what keeps "Windows-first, cross-platform later" a swap rather than a rewrite — see `CLAUDE.md` for the do-not-violate version of this rule.

**State bridge:** layer stack state, live theme selection, and any UI-visible audio-derived data (if the panel shows anything live-reactive) are marshaled across the C++/JS boundary via message-passing (`postMessage`/`ExecuteScript`), using the same JSON shape as layer preset persistence (§9) where applicable, rather than inventing a second serialization format. Theme selection itself doesn't need this — it's a simple identifier, not a schema of its own.

**Rejected alternative:** building all UI (including glass/chrome effects) as custom raylib widgets. Keeps the single-language principle intact but was rejected because hand-rolled shader-based chrome/glass rendering would fall well short of genuine Y2K/Aero fidelity without a large, dedicated engineering investment — the explicit goal here is not looking like a compromised approximation of the aesthetic.

**Security defaults (required behavior, not just a test criterion):** the control panel must ship with restricted navigation (no external URL loading), a Content Security Policy scoping resource loading to local app content, DevTools/remote debugging disabled in release builds, and local file access scoped to the app's own asset/data directories. These are implementation requirements for milestone 8, not optional hardening to add later — see `TESTING.md`'s Security / adversarial input section for how they're verified.

**Theme switching behavior:** selecting a new theme in the panel does not interrupt audio playback — audio continues uninterrupted throughout. The panel's own CSS/theme swap is near-instant and doesn't need special handling. The raylib window, however, pauses rendering while it loads the new theme's chrome assets (border/frame treatment, overlay styling — see the UI handoff docs for what counts as theme-dependent chrome), then resumes rendering once loaded. This is a deliberate, visible pause — chosen for simplicity over a hot-swap approach, since asset loading here is expected to be fast and infrequent (a user action, not a per-frame concern). If this pause ends up feeling wrong in practice, the natural upgrade path is a hot-swap using the same triple-buffering packed-word/CAS handoff pattern as `AudioFeatures` (§4, `CLAUDE.md` invariant #1) — not the simpler 2-slot double-buffer or blind-exchange triple-buffer originally considered here, both of which turned out to have genuine data races and were replaced: load new assets in the background, claim ownership of the shared slot via the same compare-exchange retry loop once ready, no pause at all. Not building that now — noted here so it doesn't need to be rediscovered later.

## 8. Milestones

Each numbered step below is a milestone — a checkpoint with its own completion bar, not just a sequence of work items. See `TESTING.md`'s "Milestone completion checklist" for what to verify at each one; the specific checks that apply depend on what that milestone actually touches (a milestone that adds no new threading code doesn't need a fresh TSan pass, one that adds no visuals doesn't need a human visual review, etc.).

1. **Local playback + feature extraction.** Local file playback working, feature extraction (banding, onset, envelope) computed and validated against synthetic fixtures. This is also where the audio → worker → render thread handoff is first built — concurrency checks apply here from the start, not just later.
2. **First visuals.** Waveform + spectrum bar visuals rendering from live `AudioFeatures`. Proves the render path end to end.
3. **Compositor.** Layer stacking, ordering, opacity, blend modes — built even with just the two visual types from milestone 2, since retrofitting compositing later is harder than designing it in now.
4. **Layer duplication + frequency mapping.** Duplicate operation, per-layer `freq_range`, and `reactivity_source` (global vs. band) all working together.
5. **Loopback capture.** Second audio producer feeding the same pipeline (WASAPI, per the Windows target in `CLAUDE.md`). Introduces mid-session source-switching as a new concurrency-relevant case.
6. **Remaining visual modules.** Particles, fractals, flow fields, geometric — additive once the architecture above is proven.
7. **Tint and palette.** Post-tint color pass first, then palette overrides per visual type.
8. **Control panel UI.** WebView2 window, `UIHost` interface, state bridge, layer stack controls wired to real layer state, and layer preset save/load. Theme selection applied last, once the panel functions — including the pause/resume behavior on the raylib side described above, not just the panel-side CSS swap.

## 9. Known Gaps / Open Considerations

Unresolved design questions that need an answer before their related milestone can be considered complete. (Audio/render thread separation is not listed here — that's a resolved decision, specified in `CLAUDE.md` invariant #1.)

Each gap resolves at the milestone that actually needs it, not before — deciding these upfront would mean designing against a moving target for work that's several milestones away.

| Gap | Resolves by | Why |
|---|---|---|
| Per-layer sensitivity/gain control | Milestone 4 | Distinct from the shared band envelope shape (resolved, §4) — this is a per-layer knob on top of it, and milestone 4 is where per-layer params (freq_range, reactivity_source) get built out. |
| Layer count performance ceiling | Milestone 3–4 | Becomes real once the compositor exists (3), sharper once duplication makes stacking many layers trivial (4). |
| Sample rate mismatch | Milestone 5 | Local playback alone (1) has one known rate — only matters once loopback introduces a second, different producer. |
| Layer preset persistence | Before/alongside Milestone 8 | The state bridge reuses this schema, so it can't be designed twice. |
| WebView2 Runtime distribution | Before Milestone 8 ships | A packaging decision, not a coding one, but blocks calling milestone 8 done. |
| CRT linking (static vs. dynamic) | Before Milestone 8 ships | Tied to the WebView2 Runtime distribution decision above — resolving them together avoids redundant packaging work. |
| Output/export | Unscheduled | Not required by any current milestone. Resolve only if this becomes an actual desired feature. |

### Gap details

- **Per-layer sensitivity/gain control** — the shared band envelope shape (attack/release) is resolved (§4), but a per-layer sensitivity/gain multiplier on top of it — letting a user make one specific layer more or less responsive than the raw band envelope, independent of every other layer reading the same band — is not yet designed. Natural fit alongside the other per-layer params (`freq_range`, `reactivity_source`) built in milestone 4.
- **Layer count performance ceiling** — each layer is a full offscreen texture render; nothing currently bounds stack size. Needs a documented default cap or explicit acknowledgment of perf degradation as layer count grows.
- **Sample rate mismatch between producers** — local files and loopback devices can report different native rates, and a user may switch source mid-session. Band Hz ranges can't assume a fixed rate; needs a resample-to-common-rate step rather than relying on windowing alone to tolerate variability.
- **Layer preset persistence** — no mechanism yet to save/reload a layer stack (types, params, blend modes, freq mappings) to disk. Without this, every session starts from scratch. Straightforward fix: JSON serialization of the `Layer` struct list. Note: §7's state bridge is specified to reuse this same JSON shape, so the schema needs to be defined once and shared, not designed twice — resolve this gap before or alongside building the state bridge (milestone 8), not independently. This is distinct from theme selection (§7) — a layer preset is a saved layer-stack configuration; a theme is a built-in UI skin. Don't conflate the two when designing the storage format.
- **WebView2 Runtime distribution** — WebView2 Runtime ships preinstalled on most current Windows machines but isn't guaranteed present. Not yet decided whether to rely on it being present (simplest, but can fail on an unpatched/older machine) or bundle the Evergreen Bootstrapper/fixed-version runtime as part of packaging. Needs a decision before milestone 8 is considered shippable, not just built.
- **CRT linking (static vs. dynamic)** — the C++ core links against the MSVC runtime either statically (`/MT`, baked into the exe, no install step, larger binary) or dynamically (`/MD`, smaller binary, requires the matching VC++ Redistributable present on the target machine). Static linking alone doesn't remove the need for an installer if WebView2 Runtime distribution (above) ends up requiring a bundled bootstrapper — in that case dynamic linking plus bundling the redist through the same installer is the lower-total-work option, since the installer is being built anyway. Decide alongside WebView2 Runtime distribution at milestone 8, not independently.
- **Output/export** — not yet decided whether this stays a live on-screen visualizer only, or needs a "render to video file" path. Affects whether rendering must be decoupled from real-time playback rate.

### Cross-platform (future, deferred until that push begins)

No macOS hardware is available locally. Linux testing can be set up directly, but macOS will need one of: CI runners (e.g. GitHub Actions macOS runners) for scripted build/test, a cloud Mac rental (MacStadium, AWS EC2 Mac instances) for interactive/visual spot-checks, or borrowed hardware for the human-review-required checks in `TESTING.md`. Running macOS in a VM on non-Apple hardware is not an option (violates Apple's EULA). This needs a decision before macOS implementation work starts, not after.