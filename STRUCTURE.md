# STRUCTURE.md — Directory & File Layout

```
aviz/
├── README.md
├── CLAUDE.md
├── TESTING.md
├── visualizer-design-spec.md
├── STRUCTURE.md
├── DEFINITION_OF_DONE.md
├── .gitignore
├── CMakeLists.txt
├── handoffs/
│   ├── bootstrap-prompt.txt          # first-session prompt: scaffold CMakeLists.txt,
│   │                                  # directory tree, README.md before any feature work
│   ├── testing-prompt.txt         # standalone audit prompt for a fresh agent session
│   │                                # to review test-suite integrity — see TESTING.md and
│   │                                # DEFINITION_OF_DONE.md's "Who writes the tests"
│   ├── ui-handoff-y2k.md            # Claude Design export notes for the Y2K theme —
│   │                                 # what to take from it, what to rebuild, open decisions
│   │                                 # already resolved. Read before touching web/panel/
│   │                                 # for Y2K-theme work. A matching Frutiger Aero handoff
│   │                                 # will land here once that theme is designed.
│   └── aviz Dark Y2K Theme.html          # raw Claude Design export referenced by the handoff
│                                           # above. Not deployable as-is (see the handoff doc)
│                                           # — kept for extracting exact CSS values, gradients,
│                                           # and layout dimensions while rebuilding web/panel/.
├── tsan/
│   └── CMakeLists.txt        # standalone Clang+TSan diagnostic build, committed for exact
│                              # reproducibility (flags/toolchain matter for what TSan can catch).
│                              # Not part of the main build — see TESTING.md's Concurrency
│                              # section for the run recipe. Its own build/ output is git-ignored,
│                              # same as the main build/ directory.
├── src/
│   ├── audio/           # producers (file, loopback), ring buffer,
│   │                     # feature extraction (FFT, banding, onset/envelope)
│   ├── compositor/       # layer stack, blend modes, offscreen texture management,
│   │                      # layer preset serialization (layer_preset.h/.cpp) and file I/O
│   │                      # (preset_file_io.h/.cpp), layer_types.h (VisualType, LayerBlendMode —
│   │                      # deliberately raylib-free, see spec §5), visual_param.h/.cpp
│   │                      # (per-module panel param dispatch, see spec §7)
│   ├── visuals/           # one subdirectory per visual module, plus reactivity_source.h
│   │   │                   # (ReactivitySource — deliberately raylib-free, see spec §5) and
│   │   │                   # number_clamp.h (shared safe-numeric-narrowing primitive, raylib-free,
│   │   │                   # used by panel message parsing and layer preset deserialization —
│   │   │                   # see TESTING.md's Security / adversarial input section)
│   │   ├── waveform/
│   │   ├── spectrum/
│   │   └── ...
│   ├── ui_host/            # UIHost interface + WebView2 implementation, state bridge
│   │                        # (C++ side of the C++/JS boundary — see spec §7),
│   │                        # panel_navigation_policy.h/.cpp (new-window/frame-navigation/
│   │                        # resource-request trust checks — see TESTING.md's Security
│   │                        # / adversarial input section), panel_window_request.h/.cpp
│   │                        # (parses the panel's window_drag/window_command requests,
│   │                        # handled on the panel thread — see spec §7's Window frames)
│   ├── app/               # top-level orchestration: window/init, main loop, wiring
│   │                        # (producer_host.h, audio_source_controller, window_layout_config.h),
│   │                        # plus the visualizer window's chrome: raylib_window_watcher
│   │                        # (Win32 subclass for drag hit-testing, corners, move detection),
│   │                        # visualizer_window_policy (raylib-free hit-test/corner rules),
│   │                        # and later the theme data struct, overlay drawing and
│   │                        # raylib-free button/layout logic
│   └── main.cpp
├── web/
│   └── panel/              # control panel HTML/CSS/JS (WebView2-hosted UI)
│       ├── themes/          # built-in theme definitions as CSS custom-property token files
│       │                     # (currently dark-y2k.css only), loaded before app.css, which uses
│       │                     # var() only. Lives here, not under assets/, because the panel's
│       │                     # virtual host maps only web/panel. Not to be confused with layer
│       │                     # presets, which are user-created and live outside the repo in the
│       │                     # runtime presets/ directory.
│       └── fonts/            # the panel's bundled font files and their licenses
├── tests/
│   ├── audio/             # mirrors src/audio — FFT banding, onset, envelope tests
│   ├── compositor/         # layer stack serialization, blend logic tests, layer preset
│   │                        # serialization/adversarial tests, preset file I/O tests
│   ├── visuals/             # mirrors src/visuals — per-module logic tests (e.g.
│   │                         # spectrum_visual_test.cpp, waveform_visual_test.cpp)
│   ├── ui_host/             # state bridge serialization/message-format tests
│   ├── app/                # mirrors src/app — producer_host/audio_source_controller/
│   │                         # window_layout_config tests
│   └── fixtures/            # synthetic signal generators, known test tones/impulses
├── assets/
│   ├── shaders/            # custom shaders (mandelbrot.fs, the Fractal module's GLSL shader — see spec §6)
│   ├── fonts/               # font files for text drawn in the raylib window (visualizer chrome
│   │                         # overlays), copied next to the executable after the build. The
│   │                         # panel's own fonts live in web/panel/fonts/. The raylib-side theme
│   │                         # data is a struct in code under src/app, not a file here; the panel's
│   │                         # theme tokens are in web/panel/themes/.
│   └── test-audio/           # pre-recorded fixture files, distinct from synthetic-generated
│                               # ones. Contents are git-ignored (unreleased personal material) —
│                               # directory itself is kept via .gitkeep. See Conventions below.
└── third_party/               # only if a dependency isn't pulled via CMake FetchContent
```

**`presets/` is a runtime-only directory, not part of the repo tree above.** Created next to each built executable (per spec §7's portable, no-install packaging decision — there's no OS-provided app-data location to rely on; practically this means a separate `presets/` per build configuration, e.g. `build/Debug/presets`, `build/Release/presets`), holding user-created layer preset JSON files (`layer_preset.h`/`.cpp`'s schema). The path is resolved once at startup from the executable's own location, not the working directory, and passed in explicitly to save and to the preset load dialog's starting folder rather than re-derived in each of those (milestone 8 fix). Not checked into the repo — git-ignored, since it's generated at runtime and may contain a user's own saved presets. The panel's preset save/load (spec §7) is the only path that reads or writes it; the debug tool and its fixed debug preset filename were removed at milestone 8.

## Build options

- **`AVIZ_VERBOSE_RAYLIB_LOG`** (CMake option, default `OFF`) — controls raylib's own trace log verbosity, not application logging. raylib's default `INFO` level prints a line for every texture/FBO load and unload, which is noisy during normal development (especially with the debug control tools reprinting a layer list after every action). Default build suppresses raylib's log to `WARNING` and above via `SetTraceLogLevel`; pass `-DAVIZ_VERBOSE_RAYLIB_LOG=ON` on the configure line to restore full `INFO`-level output when actually debugging a texture/FBO/GPU issue specifically.

  ```
  cmake -B build -G "Visual Studio 18 2026" -DAVIZ_VERBOSE_RAYLIB_LOG=ON
  ```

  This is not a general application logging framework — there's no leveled TRACE/DEBUG/INFO/WARN/ERROR system for the project's own diagnostics, and none is needed right now. The debug control tools' layer-list printouts are deliberate feature output, not logging, and aren't affected by this flag. Don't build out a broader logging framework speculatively; this flag exists to solve the one specific noise source (raylib's own trace log) that's actually come up.

## Dependency policy

- **Out-of-source builds.** Build into a `build/` directory (`cmake -B build`), never in-tree. `build/` is git-ignored.
- **Pin to a tag or commit, not a tracking branch.** `CMakeLists.txt`'s `GIT_TAG` should reference a specific release tag or commit hash for miniaudio and raylib, not `master`/`main`. This keeps builds reproducible across sessions — an agent picking this up weeks later shouldn't get a different dependency version than what was last verified working.
- **If a fetched dependency's API has changed** (e.g. after intentionally bumping a pinned version), don't silently patch around the new API to make it compile. Flag the break, describe what changed, and let the actual fix be a reviewed decision rather than an incidental one — this follows the same "flag, don't silently work around" principle used elsewhere in `CLAUDE.md`.
- **New dependencies** (beyond miniaudio, raylib, nlohmann/json, and the WebView2 SDK — all four already fetched via CMake) go through `CMakeLists.txt` FetchContent by default; only fall back to vendoring in `third_party/` if a dependency genuinely can't be fetched that way, per the existing note above.
- **Testing: GoogleTest + GoogleMock**, pinned to a release tag via FetchContent, same as miniaudio/raylib. Tests are registered with CMake's CTest (`enable_testing()` + `gtest_discover_tests()` or `add_test()`), so the full suite runs via `ctest` from the build directory — no separate test runner to install or learn. See `TESTING.md` for what gets tested and how often.
- **FFT: pffft** (`marton78/pffft`), pinned to `v1.1.0` via FetchContent, same pin-to-tag discipline as the other dependencies. Chosen over KissFFT for real-valued FFT support and SIMD optimization, which matter on the hot audio-processing path per `CLAUDE.md`'s performance-priority guideline; KissFFT was the documented fallback if pffft had no stable tagged release, which it does, so pffft is the pick. Build configuration: **float precision, SSE enabled, AVX and NEON both off.** Float matches miniaudio's sample format and raylib's GPU-side expectations; double buys precision this project doesn't need at real cost (memory bandwidth, fewer SIMD lanes per register). SSE is a safe baseline present on every x86-64 CPU for 20+ years, so it can be compiled in unconditionally with no runtime feature detection. AVX is left off — not guaranteed present on older CPUs, and enabling it safely would require runtime detection and a fallback path, which is unnecessary complexity at these FFT sizes (1024-point). NEON is ARM-only and irrelevant until the cross-platform push (spec §9) targets Apple Silicon.

## Conventions

- **`tests/` mirrors `src/` 1:1.** Makes it obvious where a new module's tests belong, and a module directory with no matching test directory is a visible gap rather than a silent one.
- **One directory per visual module, not one file.** Even simple visuals will likely want params/shader/logic separated as they grow. This also keeps a visual module's expected shape concrete and checkable.
- **No top-level `include/`.** This is a single-binary app, not a library with external consumers, so headers live alongside their `.cpp` files in the same module directory rather than behind a separate public-include split.
- **`assets/test-audio/` vs. `tests/fixtures/` are conceptually distinct**, even though both serve testing:
  - `tests/fixtures/` — synthetic, code-generated signals (exact frequencies, exact impulse timing) used in automated unit tests.
  - `assets/test-audio/` — real recorded files for human "does this sound/look reasonable on real music" spot-checks per `TESTING.md`. Not asserted on programmatically. **Contents are git-ignored**, not committed — the files in use are unreleased personal material, so they stay local rather than entering git history. The directory itself is kept present (via `.gitkeep` or equivalent) so `STRUCTURE.md`'s layout still holds even with no tracked files inside.
- **`web/panel/` (the WebView2 UI) is not covered by GoogleTest/CTest.** `tests/ui_host/` covers only the C++ side of the state bridge (message format, serialization). No JS test framework has been chosen yet — until one is, panel behavior (theming, drag-reorder, layout) is human-review territory, same as visual/audio correctness in `TESTING.md`. Flag this rather than silently picking a JS framework.
- **A shared, cross-module utility that must stay raylib-free (e.g. `number_clamp.h`) lives in `src/visuals/` alongside `reactivity_source.h`, not in a new top-level `utils/`.** This project deliberately has no top-level `include/` (above), and a `utils/` dump would be the same kind of ungrounded catch-all — `src/visuals/` already holds raylib-free shared headers used outside the directory's own namesake modules, so a new one of the same kind joins them there rather than establishing a second location.
- **`handoffs/` is reference material, not part of the load-bearing doc set.** Unlike `CLAUDE.md`/`TESTING.md`/`STRUCTURE.md`/the design spec/`DEFINITION_OF_DONE.md`, files here aren't kept in sync automatically and don't get the "flag if stale" treatment — they're point-in-time notes (a design export's quirks, an audit prompt) meant to be read once at the relevant point in the project, not treated as ongoing architecture truth. If something in a handoff conflicts with the core docs, the core docs win.

## Git conventions

- Commit at the end of each unit of work, not just at milestone boundaries — smaller commits make it far easier to isolate exactly where something went wrong if it does, and milestones in this project can span a lot of work.
- The initial commit (docs only, before any code exists) is the clean baseline everything else diffs against — don't fold early scaffolding work into that same commit.