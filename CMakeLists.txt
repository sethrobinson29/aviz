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
  src/app/layer_generator.cpp
  src/audio/miniaudio_backend.cpp
  src/audio/ring_buffer.cpp
  src/audio/feature_extractor.cpp
  src/audio/feature_worker.cpp
  src/audio/file_producer.cpp
  src/audio/loopback_producer.cpp
  src/audio/pipeline_format_converter.cpp
  src/visuals/waveform/waveform_visual.cpp
  src/visuals/spectrum/spectrum_visual.cpp
  src/visuals/radial/radial_visual.cpp
  src/visuals/particles/particles_visual.cpp
  src/compositor/layer.cpp
  src/compositor/compositor.cpp
  src/compositor/compositor_math.cpp
)
target_include_directories(aviz PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(aviz PRIVATE miniaudio raylib PFFFT)
if(AVIZ_VERBOSE_RAYLIB_LOG)
  target_compile_definitions(aviz PRIVATE AVIZ_VERBOSE_RAYLIB_LOG)
endif()

# --- tests ---------------------------------------------------------------------
add_subdirectory(tests)
