# TESTING.md — Verification Strategy

How correctness gets checked in this project, and which parts require a human vs. which an agent can verify on its own.

## Pure-logic pieces — unit tests + synthetic fixtures

Deterministic, no listening or watching required. Cover these with real unit tests:

- FFT banding (does a known input frequency land in the expected band?)
- Onset/beat detection (does a synthetic impulse train trigger onsets at the right times?)
- Envelope smoothing (attack/release behavior against known input curves)
- Layer stack serialization (save/load round-trips correctly)

Use synthetic signal fixtures for anything frequency- or timing-dependent — e.g., a generated sine wave at a known frequency to validate banding, a synthetic kick-only impulse train and a synthetic hi-hat-only impulse train to validate that per-band onset detection actually behaves differently per band (this is the core claim behind having per-band detection at all, so it should be checkable in isolation, not just assumed to work).

**Pure logic co-located with threading code is not exempt from unit tests.** The Concurrency section below carves out thread orchestration/handoff glue from routine unit-test coverage — that carve-out is about the orchestration itself (thread lifecycle, signaling, ownership handoff), not a blanket exemption for every function that happens to live in the same file. If a file mixing threading glue and pure computation exists (e.g., a worker's drain/processing step that also does windowing or mixdown math with no actual dependency on threads), extract the pure-logic part into a unit that can be called and tested directly, without spinning up a worker thread — don't let it fall through the gap between "not concurrency-only, so not covered by TSan/stress" and "not obviously a standalone module, so nobody wrote a unit test for it either."

**Regression tests are named for what they guard against, not commented for it.** A test written to catch a specific bug from recurring should have that provenance in its name (e.g. `LowFrequencyBandsDoNotOverlap`, not `BandEdgeTest3`) — consistent with `CLAUDE.md`'s comment rules, which prefer a clear name over a comment explaining an unclear one, and which don't allow a comment that states why a piece of code exists rather than what it does. A test name is not subject to the same "what, not why" restriction a code comment is — a name that encodes provenance is a good name, not a comment in disguise — but a comment on the test explaining the same thing would be redundant with a well-chosen name and shouldn't be added alongside it.

## Visual correctness — human review via actual playback, not static output

An agent cannot see the screen and should not claim a visual "looks right." Rendering/visual changes are reviewed by running the app and watching actual playback — that's the real check, not a substitute.

A single rendered frame or image dump is not sufficient evidence of correctness: it can't show motion, timing, or whether a layer is actually reacting to the live signal. If useful as a supplementary record of a change, the agent can render a short video clip (a few seconds) for reference, but this supplements human review — it does not replace it.

## Audio correctness — human review via actual playback, with logging as a supplement

Same principle as visual correctness: whether playback actually sounds right is checked by a human listening to it running, not inferred by the agent.

Underrun/overrun counts from miniaudio should still be logged — they're cheap to capture and catch gross problems (dropped buffers, glitches) immediately. But they're a supplementary automated signal, not a substitute for listening. Don't report "audio pipeline works" based on clean underrun/overrun counts alone.

**Verifying onset/feature detection by ear before there's a visual to watch (relevant to milestone 1 specifically):** listening to real playback and cross-checking against a live debug printout (RMS, band energies, onset flags) is a legitimate way to sanity-check detection before milestone 2 gives you an actual visual response to eyeball. Two things make a naive version of this misleading, worth building in from the start rather than rediscovering:

- **Onset flags are single-hop-wide** (~10ms at hop 512/48kHz) and easy to sample-miss if debug output prints on a slower fixed interval — e.g. a 250ms print cycle has roughly a 1-in-23 chance of landing on any given onset's one true hop. This makes correct detection look sporadic/broken even when it isn't. Fix: latch the onset flag (sticky since the last print, cleared after each print) rather than sampling instantaneous state, so the debug output reflects "did an onset fire since last shown" instead of "is an onset true at this exact instant."
- **A short warm-up period with no output is expected, not a bug.** Onset detection needs a full trailing window of history (~92 hops, ~1 sec) before its adaptive threshold is meaningful — nothing should fire in that window regardless of what's playing.
- **A live debug flag is a rough indicator, not proof of correctness either way.** The actual correctness check is the synthetic-fixture test (`KickAndHiHatFireDistinctBands` or equivalent) — it records onsets across a whole clip with known ground truth, rather than a human's real-time impression of a real track. Don't skip the automated test on the theory that a listening session already confirmed it; they're checking different things (does detection generally seem sensible vs. does it fire at the exact expected positions).
- Keep the automated onset test on **synthetic tones, not real files** — real material has no exact ground truth for expected onset positions, so switching the test to real audio would trade a precise, assertable check for a fuzzy one. If a tool for logging real-file onsets to a file for easier review would help a listening session, build it as a separate utility, not a modification to the existing correctness test.

## No-hardware fallback

Not applicable to this project. Dev and test agents both run on the same real Windows machine with real audio hardware and a real display always present — there's no sandboxed/hardware-less environment in this workflow, no CI, and no plan to distribute an executable.

`tests/fixtures/synthetic_signal.h` remains in use as a unit-test fixture (known-frequency sine waves, known-timing impulse trains, for asserting exact expected behavior) — that's unrelated to hardware availability and stays as-is.

## Test cadence

- **After touching logic covered by an existing or new test**, run that test (or its test file/suite) immediately — don't wait until the end of a larger task to find out something broke.
- **Before marking any unit of work done** (per `DEFINITION_OF_DONE.md`), run the full test suite (`ctest` from the build directory), not just the tests you expect to be affected. Catching an unrelated regression here is the point.
- Tests are not run on a timer or in CI for this project — there's no background process catching regressions between sessions. That makes the "full suite before done" step non-optional rather than a nice-to-have.

## Security / adversarial input

Correctness testing (above) checks "does it work." This section covers "does it fail safely" — the property that matters at trust boundaries, where input isn't assumed to be well-formed.

**File decoding and layer preset deserialization.** These are the two boundaries where untrusted or malformed input reaches the C++ core: a malformed audio file feeding miniaudio's decoder, and a hand-edited or corrupted layer preset JSON file. The property to test is "rejects cleanly" — no crash, no out-of-bounds read, no silently trusting an out-of-range value. Concretely: don't propagate a `freq_range` with a negative `min_hz` or a `max_hz` above Nyquist into FFT bin math without clamping or rejecting it first; validate types and ranges on every field read from a layer preset file rather than assuming the JSON matches the expected shape. Cover this with a handful of deliberately malformed fixture files at each boundary — this doesn't need a full fuzzing harness for a solo project, but explicit negative-path tests at both boundaries are worth the cost. `EXPECT_DEATH`-style tests are useful here for asserting a controlled abort rather than corrupted continued execution, where that's the intended failure mode.

Note the trust asymmetry: built-in themes (`assets/themes/`) ship with the app and are effectively trusted, while layer presets are user-created/user-edited files and should be treated as untrusted input regardless of how they were originally produced.

**The WebView2/JS boundary — offline reduces but does not eliminate this.** Running without an internet connection shrinks the attack surface a lot (no remote content, no network-based exploitation in normal operation) but the boundary itself still exists and is worth treating deliberately rather than assuming "offline" means "safe":

- **The state bridge is a trust boundary in both directions.** The C++ side must validate/sanitize anything the JS side sends before acting on it (same discipline as the layer preset file boundary above) — don't assume a message from the panel is well-formed just because it came from your own UI code. Conversely, JS must not render untrusted strings (layer names, layer preset names, anything loaded from a file) into the DOM without escaping — an unsanitized layer name containing markup is a local XSS vector even in a fully offline app, since it still executes in the WebView2 context.
- **Restrict outbound navigation and network requests from the panel explicitly**, via WebView2's navigation/resource-request event handlers — don't rely on "the app is offline" as the control. A malicious or corrupted theme or layer preset file could contain a remote URL (image `src`, CSS `@import`, script `src`); if the machine ever has connectivity, that request could fire. Treat network restriction as defense in depth, not a redundant step.
- **Disable DevTools/remote debugging in release builds.** WebView2 can expose a remote debugging port in some configurations — this is a local attack vector that should be off by default outside of active development.
- **Set a Content Security Policy scoping script/style/resource loading to local app content only**, no remote origins, so even a successful injection has nowhere useful to load from.
- **Scope local file access.** The panel shouldn't be able to read arbitrary filesystem paths — only its own bundled assets and the app's designated layer preset/theme data directories.

## Milestone completion checklist

Each milestone in `visualizer-design-spec.md` §8 is a checkpoint, not just a work item. Before considering a milestone finished, check the boxes below that actually apply to what that milestone touched — don't run checks for systems the milestone didn't change:

- **Always:** full test suite passes (`ctest` from the build directory).
- **If the milestone added or changed threading/handoff code** (audio → worker → render): run a Clang+TSan diagnostic build under WSL2, and an extended stress-test run per the Concurrency section below.
- **If the milestone changed audio behavior** (new producer, new feature extraction, format handling): human review of actual playback per the Audio correctness section above.
- **If the milestone changed rendering/visual behavior**: human review of actual running playback per the Visual correctness section above.
- **If the milestone changed the control panel UI** (`web/panel/`, `ui_host/`, state bridge): human review of the actual running panel (layout, theme switching, drag-reorder, fullscreen toggle) — no JS test framework is chosen yet, so this is human-review territory the same way visual/audio correctness is. `tests/ui_host/` unit tests still apply to the C++-side state bridge logic.
- **If the milestone added or changed file/layer-preset parsing, or the control panel UI**: adversarial input coverage per the Security / adversarial input section above — malformed fixture files for parsing changes, state-bridge validation and navigation/CSP restrictions for UI changes.
- **Always:** `README.md` updated to reflect current functionality, per `DEFINITION_OF_DONE.md`.
- **Always:** any architecture docs the milestone made stale are flagged, per `DEFINITION_OF_DONE.md`.

A milestone that only adds a new visual module with no threading or audio changes, for example, needs the full test suite, a visual review, and a README update — not a fresh TSan pass.

## Concurrency

The audio → worker → render thread handoff (`CLAUDE.md` invariant #1) is the part of this codebase most likely to have race-condition bugs rather than wrong-output bugs — and races don't reliably show up in a normal GoogleTest run. This section covers the orchestration/handoff itself; pure logic that happens to live alongside it (e.g. windowing/mixdown math inside a worker's processing step) still needs direct unit test coverage per the note above, not a pass under this section's carve-out.

- **Prevent structurally first.** Single-producer/single-consumer lock-free ring buffer for raw PCM; `AudioFeatures` snapshot published via triple buffering (worker's private back slot, render's private front slot, shared middle slot claimed via a packed index+flag atomic word and a compare-exchange retry loop on the consumer side — spec §4, `CLAUDE.md` invariant #1), not a bare 2-slot double-buffer swap and not a blind atomic exchange on the shared slot. No locks in the audio callback, ever. Getting this right by design matters more than catching races after the fact.

**Track record (why this section isn't theoretical):** the ring buffer's overwrite-oldest handling, the `AudioFeatures` snapshot swap, and — within the snapshot fix itself — the first attempted repair all shipped with genuine data races, each found by exactly the process this section specifies (a stress test, 2000-iteration forced-overflow repeats, plus a fresh-context test-suite audit) rather than by inspection or by TSan alone. The ring buffer race was fixed with relaxed atomics on slot storage. The snapshot race took two structural iterations, not one: the original 2-slot double-buffer was replaced with triple buffering using a blind atomic exchange on the shared slot, which the same stress test then also caught as a regression — the fix was a packed index+new-data-flag atomic word claimed via a compare-exchange retry loop on the consumer side, not a blind exchange. Treat "the stress test didn't reproduce a failure this run" as weak evidence at best, per the reasoning above — it wasn't sufficient to rule out any of these three bugs and shouldn't be treated as sufficient on future threading changes either.
- **Diagnostic builds, at relevant milestones only.** ThreadSanitizer doesn't work well with MSVC on native Windows (the primary target — see `CLAUDE.md`). When a milestone adds or changes threading code, run the committed standalone build in `tsan/` under WSL2 to catch races, per the milestone completion checklist above. This is a separate, occasional build config — not part of the regular Windows dev loop, and not built with the main project's `CMakeLists.txt`.

  **Recipe** (from WSL2, in `tsan/`):
  ```bash
  cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
  cmake --build build
  ```
  Compile flags: `-fsanitize=thread -g -O1` (`-O1` because TSan's runtime needs real, not heavily-optimized-away, memory accesses to instrument; `-g` for symbolized stack traces in any race report). Link flag: `-fsanitize=thread`. Both are baked unconditionally into `tsan/CMakeLists.txt` — there's no toggle, since this project exists only to be the TSan build.

  Run the full suite once, then a targeted repeat pass on concurrency tests specifically:
  ```bash
  ./build/audio_tests_tsan
  TSAN_OPTIONS='halt_on_error=1' ./build/audio_tests_tsan --gtest_filter='*Concurrency*' --gtest_repeat=2000 --gtest_brief=1
  ```
  Pipe output to a log and check for `WARNING: ThreadSanitizer` or nonzero `FAILED` counts — either means a real race, not a flake. This doesn't go through `ctest` (the standalone `tsan/CMakeLists.txt` never calls `enable_testing()`); the binary is run directly.

  `tsan/CMakeLists.txt`'s source list must be kept in sync with the main project's audio test target by hand — it's a separate build, so it doesn't automatically track additions/removals from the real `tests/audio/` suite. If a file's been added there since the last TSan run, add it to `tsan/CMakeLists.txt` too before trusting the result.
- **Stress testing, at relevant milestones only.** Races are often load-dependent and probabilistic — a single short test run may not reproduce one. When a milestone touches the thread handoff, run the pipeline continuously (real file playback, looped for an extended duration — there's no app-level synthetic producer; see the No-hardware fallback section) and watch for crashes, hangs, or corrupted `AudioFeatures` output.

## Summary

| Piece | Verification method |
|---|---|
| FFT banding, onset detection, envelope smoothing, serialization | Unit tests + synthetic fixtures |
| Visual/rendering changes | Human review of actual running playback |
| Audio playback correctness | Human review of actual running playback |
| Buffer underrun/overrun | Logged automatically, supplementary only |
| Thread handoff races | Structural prevention (lock-free ring buffer, atomic snapshot swap) + periodic Clang/TSan diagnostic build + periodic stress test, not routine per-change checks |
| Control panel UI (`web/panel/`) behavior | Human review of actual running panel; no JS test framework chosen yet. C++-side state bridge (`ui_host/`) still gets unit tests. |
| Malformed audio files / layer preset JSON | Malformed fixture files at both boundaries; reject cleanly, no crash/OOB read, no unclamped ranges into FFT math |
| WebView2/JS trust boundary (state bridge, navigation, local file access) | Bidirectional state-bridge validation, restricted navigation/network + CSP, DevTools off in release, scoped local file access — treated as real even offline |