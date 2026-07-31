# DEFINITION_OF_DONE.md

Completion criteria for different kinds of work in this project, and what happens to documentation when a change is made.

## Visual module

A new visual module is done when it:

- Consumes `AudioFeatures` per its configured `reactivity_source` and `freq_range` (see design spec §4–5).
- Exposes its `visual_params` for the compositor/UI to set.
- Renders to its own offscreen texture per the compositor contract (design spec §5).
- Has any unit-testable logic beyond rendering (e.g. parameter math, band-response curves) covered per `TESTING.md`.

Partial wiring is not done. If a module renders but ignores `freq_range`, or has params that aren't actually connected, say so explicitly rather than presenting it as complete.

## Pipeline / architecture change

Done when it:

- Doesn't violate the invariants in `CLAUDE.md`.
- Has unit tests updated or added where logic changed.
- If it touches audio or rendering behavior, has been confirmed via actual playback per `TESTING.md` — "it compiles" is not sufficient evidence.

## Control panel / UI work

A control panel feature (a new panel control, theme selection wiring, layer preset save/load, state bridge message, etc.) is done when it:

- Is wired to real layer/theme state through the defined state bridge (design spec §7) — no UI element that looks functional but isn't actually connected.
- Ships with the required security defaults already in place for anything it touches (restricted navigation, CSP, DevTools off in release, scoped file access) — these aren't a separate follow-up task, per `CLAUDE.md` invariant #8.
- Has been reviewed by running the actual panel, per `TESTING.md` — no JS test framework exists yet, so this is the verification method, not a placeholder for one.
- If it parses or sends data across the state bridge, has adversarial-input coverage per `TESTING.md`'s Security / adversarial input section (malformed fixture messages, validated on both sides of the bridge).
- Any C++-side logic behind it (state bridge serialization, validation) has unit test coverage per the pipeline/architecture change criteria above.

## Bug fix

Done when the original failure mode is covered by a test, or by a documented manual-verification step if it isn't testable — so it can't silently regress.

## Who writes the tests

Writing tests is part of the unit of work, not a follow-up step. If a change requires test coverage per the criteria above, the same agent doing the implementation writes those tests as part of finishing it — not deferred, not left for a human to write later.

Tests must actually exercise the behavior they claim to check. A test that's written to pass regardless of whether the underlying code works — hardcoding an expected result to match whatever the code currently returns, asserting a tautology, or trivially satisfying the assertion instead of exercising real logic — is not a valid test and does not count toward done. If a test can't meaningfully fail when the code it targets is broken, it isn't verifying anything and shouldn't be written as if it does.

## Documentation on completion

Two different update policies depending on the document:

- **`README.md` (user-facing)** — update immediately, as part of "done," not deferred to later. It documents current functionality/build steps/controls, not a design decision, so it should always reflect what the app actually does right now. Treat it closer to a build artifact than a design document.
- **`CLAUDE.md`, `TESTING.md`, `STRUCTURE.md`, `visualizer-design-spec.md`, `DEFINITION_OF_DONE.md` (agent/contributor-facing)** — do not edit automatically, per the existing rule in `CLAUDE.md`. If a change makes one of these stale or inaccurate, flag it explicitly as part of finishing the work, rather than silently leaving it stale or silently correcting it. These encode decisions, not just current state, and an automatic edit risks overwriting a deliberate decision.
