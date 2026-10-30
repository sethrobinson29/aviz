cmake_minimum_required(VERSION 3.24)
project(aviz CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AVIZ_VERBOSE_RAYLIB_LOG
  "Show raylib's own INFO-level trace log (every texture/FBO load and unload) instead of suppressing it to WARNING and above. See STRUCTURE.md's Build options."
  OFF)

include(FetchContent)

# --- miniaudio (header-only) ------------------------------------------------
# Populated (not add_subdirectory'd) — miniaudio's own CMakeLists.txt builds
# its test/example suite (with submodule deps we don't want); we only need
# miniaudio.h/miniaudio.c as a single-header library.
FetchContent_Declare(
  miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG 0.11.25
)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
  FetchContent_Populate(miniaudio)
endif()

add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})

# --- raylib ------------------------------------------------------------------
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 6.0
)
FetchContent_MakeAvailable(raylib)

# --- pffft --------------------------------------------------------------------
# Float-only; SIMD left on with pffft's own default arch level ("none" —
# no explicit /arch: flag), which on MSVC x64 means the mandatory SSE2
# baseline with no AVX opt-in. Tests/benchmarks/examples default OFF when
# consumed as a subdirectory dependency, per pffft's own CMakeLists.txt.
set(PFFFT_USE_TYPE_FLOAT ON CACHE BOOL "" FORCE)
set(PFFFT_USE_TYPE_DOUBLE OFF CACHE BOOL "" FORCE)
set(PFFFT_USE_SIMD ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  pffft
  GIT_REPOSITORY https://github.com/marton78/pffft.git
  GIT_TAG v1.1.0
)
FetchContent_MakeAvailable(pffft)

# --- nlohmann/json --------------------------------------------------------------
# Layer preset persistence and, later, the WebView2 state bridge (spec §7/§9)
# both serialize the same JSON shape. Header-only, chosen for idiomatic
# modern-C++ ergonomics over RapidJSON/simdjson -- preset files are small and
# read/written rarely, so neither of those libraries' performance edge matters
# here, and this one reads closest to normal C++ at every call site.
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# --- WebView2 SDK (headers + static loader lib for the control panel window,
# milestone 8, spec §7) --------------------------------------------------------
# Headers/lib only -- the WebView2 *runtime* itself is resolved from the
# system-installed Evergreen runtime for now (not bundled). Fixed Version
# runtime bundling (CLAUDE.md's no-install packaging decision) is deferred to
# a dedicated packaging pass, not part of this slice. Fetched directly by URL
# (the NuGet package is just a zip) rather than via vcpkg or a nuget.exe CLI
# step, per STRUCTURE.md's "new deps go through FetchContent" policy and the
# same pin-to-exact-version discipline as the other dependencies above.
# WebView2LoaderStatic.lib (not WebView2Loader.dll) is used so no extra DLL
# needs to ship next to the exe, consistent with static CRT linking (/MT).
# Windows-only; guarded below where it's actually consumed.
if(WIN32)
  FetchContent_Declare(
    webview2sdk
    URL https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.4191.47/microsoft.web.webview2.1.0.4191.47.nupkg
    URL_HASH SHA256=f492bbf547d0da329553b6727435b677579b1e9f91cc9e4a1ad029366d5f23d0
    DOWNLOAD_NAME webview2sdk.zip
  )
  FetchContent_GetProperties(webview2sdk)
  if(NOT webview2sdk_POPULATED)
    FetchContent_Populate(webview2sdk)
  endif()

  add_library(webview2sdk INTERFACE)
  target_include_directories(webview2sdk INTERFACE ${webview2sdk_SOURCE_DIR}/build/native/include)
  target_link_libraries(webview2sdk INTERFACE
    ${webview2sdk_SOURCE_DIR}/build/native/x64/WebView2LoaderStatic.lib)
endif()

# --- GoogleTest / GoogleMock ---------------------------------------------------
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

# --- app ---------------------------------------------------------------------
add_executable(aviz
  src/main.cpp
  src/app/app.cpp
  src/app/audio_source_controller.cpp
  src/app/layer_generator.cpp
  src/app/window_layout_config.cpp
  src/app/window_layout_controller.cpp
  src/app/window_mode_state.cpp
  src/audio/miniaudio_backend.cpp
  src/audio/ring_buffer.cpp
  src/audio/feature_extractor.cpp
  src/audio/feature_worker.cpp
  src/audio/audio_file_validation.cpp
  src/audio/file_producer.cpp
  src/audio/loopback_producer.cpp
  src/audio/pipeline_format_converter.cpp
  src/visuals/palette.cpp
  src/visuals/waveform/waveform_visual.cpp
  src/visuals/spectrum/spectrum_visual.cpp
  src/visuals/radial/radial_visual.cpp
  src/visuals/particles/particles_visual.cpp
  src/visuals/lissajous/lissajous_visual.cpp
  src/visuals/flow_field/flow_field_visual.cpp
  src/visuals/fractal/fractal_visual.cpp
  src/compositor/layer.cpp
  src/compositor/compositor.cpp
  src/compositor/compositor_math.cpp
  src/compositor/layer_preset.cpp
  src/compositor/preset_file_io.cpp
  src/compositor/visual_param.cpp
  src/ui_host/panel_message.cpp
  src/ui_host/panel_dispatch_queue.cpp
  src/ui_host/panel_message_apply.cpp
  src/ui_host/panel_navigation_policy.cpp
)
target_include_directories(aviz PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(aviz PRIVATE miniaudio raylib PFFFT nlohmann_json::nlohmann_json)
if(AVIZ_VERBOSE_RAYLIB_LOG)
  target_compile_definitions(aviz PRIVATE AVIZ_VERBOSE_RAYLIB_LOG)
endif()

# UIHost's WebView2 implementation is Windows-only; the interface itself
# (src/ui_host/ui_host.h) has no platform code and is always compiled in
# above via panel_message.cpp/panel_dispatch_queue.cpp. See CLAUDE.md on the
# UIHost interface being designed swappable even though only Windows is
# implemented now.
if(WIN32)
  target_sources(aviz PRIVATE src/ui_host/ui_host_webview2.cpp src/app/raylib_window_watcher.cpp)
  target_link_libraries(aviz PRIVATE webview2sdk Ole32 Shlwapi Comctl32 Comdlg32)
endif()

add_custom_command(TARGET aviz POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets/shaders
    $<TARGET_FILE_DIR:aviz>/assets/shaders
  COMMENT "Copying assets/shaders next to aviz executable")

add_custom_command(TARGET aviz POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/web/panel
    $<TARGET_FILE_DIR:aviz>/web/panel
  COMMENT "Copying web/panel next to aviz executable")

# --- tests ---------------------------------------------------------------------
add_subdirectory(tests)
