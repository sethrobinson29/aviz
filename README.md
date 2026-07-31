# aviz — Music Visualizer

Real-time, layered audio-reactive music visualizer. Audio comes from local
files or system audio loopback (not direct streaming-service extraction —
see `visualizer-design-spec.md` §3). Visuals are independent modules
composited into a stack that can be reordered, blended, tinted, and
duplicated. See `visualizer-design-spec.md` for full architecture and
`CLAUDE.md` for the working invariants.

## Status

Bootstrapping only — no audio pipeline, visuals, or UI implemented yet.
Build order follows `visualizer-design-spec.md` §8; milestone 1 (local
playback + feature extraction) is next.

## Stack

- C++17 core
- [miniaudio](https://github.com/mackron/miniaudio) — audio I/O and loopback capture
- [raylib](https://github.com/raysan5/raylib) — rendering
- WebView2 (HTML/CSS/JS) — control panel UI (not yet built)

## Platform

Windows only, for now. Build natively with MSVC + the Visual Studio
generator — not through WSL2. See `CLAUDE.md`'s Target Platform section.

## Building

Requires CMake 3.24+ and Visual Studio 2022 (or compatible) with the
"Desktop development with C++" workload.

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build
```

Dependencies (miniaudio, raylib, GoogleTest/GoogleMock) are fetched and
pinned to specific release tags via CMake FetchContent — no manual
installation needed.

## Testing

```
ctest --test-dir build
```

See `TESTING.md` for what's covered by automated tests versus human review.

## Features

None yet — bootstrapping stage. The current executable only confirms the
toolchain and dependencies link and run.
