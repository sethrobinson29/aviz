# UI Handoff — Y2K Theme (Claude Design Export)

Source: `Y2K Visualizer Layer UI.dc.html`, exported from Claude Design. Read this handoff before touching `web/panel/`.

## What this file is, and isn't

This is a Claude Design component spec, not a deployable HTML/CSS/JS export. It depends on a missing runtime (`support.js`), uses that runtime's template syntax (`{{ }}` placeholders, `<sc-if>`/`<sc-for>` directives, `style-hover`/`style-active` attributes) which are not valid HTML/CSS, and won't render in a real browser or WebView2 as-is. Do not attempt to load it directly or treat it as a working starting point — it needs to be rebuilt as real HTML/CSS/vanilla JS (or whatever `web/panel/` ultimately standardizes on), using this file as a design and behavior reference.

## What to take from it directly (visual language)

- **Color palette, gradients, and chrome/glass treatment** — the metallic button gradients, glossy slider thumb styling, accent-color glow effects, and the angular clip-path panel shapes are the actual Y2K aesthetic decisions and should carry over as real CSS.
- **Typography** — Audiowide (headers/logo), Orbitron (labels/buttons), Inter (body), IBM Plex Mono (numeric readouts). These are loaded from Google Fonts in the source file — **do not keep the network font-loading call**. Self-host or bundle these fonts as local files instead; loading from `fonts.googleapis.com` violates the restricted-navigation/CSP requirement in spec §7 and `CLAUDE.md` invariant #8.
- **Layout** — three-column grid in the source mockup (layer stack list / live preview + transport / inspector panel), top bar with preset selector and add-layer button. The layer-stack and inspector columns carry over to the panel as designed; the center column's chrome/framing carries over to the raylib window instead, per the two-window section below — it does not become a third panel column.
- **Iconography and component styling** — button shapes (angular clip-paths), the drag-handle dot grid, the slider/dropdown/color-swatch styling.

## What needs to be rebuilt, not copied

- **All markup** — rewritten as plain HTML with real CSS (external stylesheet or `<style>` block), no template runtime dependency.
- **All interaction logic** — rewritten as vanilla JS (or the eventual `web/panel/` standard) wired to the real state bridge (spec §7) instead of local component state. The source file's `this.setState`-based logic is a behavior reference only — see below for what to preserve from it.
- **Fonts** — self-hosted, not loaded from Google Fonts.
- **Security defaults** — CSP, restricted navigation, DevTools disabled in release, scoped file access — none of this exists in the source file and all of it is required per `CLAUDE.md` invariant #8, not optional polish.

## Behavior to preserve from the source JS (as reference, not code)

- **Layer data shape**: `{ id, name, type, opacity, blend, tint, band }` — matches our `Layer` struct's relevant fields (opacity, blend_mode, tint_color, freq_range/reactivity_source) closely enough to use as a starting point for the panel-side representation, but must ultimately serialize to/from the real `Layer` struct via the state bridge, not stand alone.
- **Blend mode mapping**: `Normal → source-over`, `Additive → lighter`, `Multiply → multiply`, `Screen → screen`. Confirms alignment with the compositor's blend modes (design spec §5) — worth carrying forward as the canonical mapping for the compositor's actual blend-mode implementation (raylib's `BlendMode` equivalents), not for an in-panel preview, since we're not building one (see two-window section below).
- **Interaction behaviors to reimplement for real**: add layer, duplicate layer, delete layer, drag-to-reorder (drop-line indicator on drag-over), select layer to populate the inspector, opacity slider, blend mode dropdown, reactivity-source dropdown, tint color swatch picker, editable layer name.

## Two-window architecture — decided, not open

The source mockup includes a live animated "Live Preview" canvas *inside* the control panel, with a REC/60fps indicator and its own canvas draw loop simulating the composited output, plus transport controls (play/pause, BPM readout, master beat meter) alongside it. Claude Design built this because the prompt didn't specify window separation — it wasn't told about the two-window architecture.

**Decision: stick with the two-window model from spec §7.** The control panel (WebView2) does not render live visuals and does not host a preview canvas. The raylib window is the only place visuals render, including fullscreen mode. Do not build a live preview into the panel.

This doesn't mean discarding that part of the mockup — it means **re-targeting it at the raylib window instead of dropping it**:

- The chrome/glass framing around the preview canvas (the metallic border, corner rivets, angular clip-path cutouts, "REC · 60fps" readout styling) is part of the Y2K visual language and should inform the raylib window's own chrome — its border/frame treatment, any on-screen overlay (e.g., the fullscreen exit button, a status readout) — rendered as textures/shaders in raylib rather than as HTML/CSS in the panel.
- The transport controls (play/pause, BPM readout, master beat meter) belong conceptually with playback, not layer management — these likely still live in the control panel as controls (since they're inputs the user acts on), but the *meter/readout visualization itself* (the animated beat bars) is a candidate for rendering in the raylib window as a genuinely audio-reactive element, using real `AudioFeatures` data instead of the mockup's fake sine-wave animation. Flag which side owns final rendering of the beat meter rather than assuming.

## Additional notes from the source file

- **The mockup exposes an `accentColor` prop with four preset options** (`#00e0ff` cyan, `#ff2ec4` pink, `#baff29` lime, `#ff8a00` orange) — this is a Claude Design authoring control, not a decided app feature. It's a signal worth considering (a Y2K accent-color sub-variant, similar to how some WinAmp skins had color variants), but it hasn't been decided as an actual requirement. Don't build accent-color switching as a feature unless it's explicitly discussed and added to spec §7 — treat it as a design idea surfaced by the tool, not a spec.
- **The mockup only defines icons/visual treatment for 4 of the 8 planned visual modules** (waveform, spectrum, particles, fractal — matching `TYPE_META` in the source). Spec §6 also lists radial/circular, Lissajous, plasma/flow fields, and geometric/abstract, none of which have a designed icon or list-row treatment yet. This isn't a blocker for early milestones (spec §8 builds those visual types later, milestone 6), but note it as follow-up design work needed before milestone 6, not something to invent ad hoc when that milestone arrives.
- **The four seeded layers in the source file's initial state** (Background Fractal, Spectrum Bars, Waveform Glow, Particle Sparks) are Claude Design's demo/preview data only — not a real default layer stack for the app. Don't carry these specific layers over as the app's actual startup state.

## Everything else

The preset dropdown in the top bar is new UI surface not yet covered by any existing doc — wire it to the state bridge the same way as layer controls. Note this dropdown is the **layer preset** selector specifically (see terminology note in spec §7) — it needs a sibling, not a replacement: a **theme** dropdown, same visual treatment, placed to its left in the top bar. Selecting a theme triggers the pause/resume behavior specified in spec §7 (raylib window pauses to load new chrome assets while audio keeps playing; panel's own CSS swap is instant and needs no special handling).