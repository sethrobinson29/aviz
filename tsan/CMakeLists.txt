# tsan/CMakeLists.txt — standalone ThreadSanitizer diagnostic build.
#
# NOT part of the main project build. This exists solely to compile the audio
# concurrency test suite under Clang + ThreadSanitizer, since TSan doesn't work
# well with MSVC on native Windows (the project's primary target — see
# CLAUDE.md). Run from WSL2. See TESTING.md's Concurrency section for the full
# recipe (configure/build/run commands, how to read results).
#
# Deliberately unconditional (no CMake option()/cache flag) — this project
# exists only to be the TSan build, so there's no "off" state to support.

cmake_minimum_required(VERSION 3.20)
project(aviz_tsan CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-fsanitize=thread -g -O1)
add_link_options(-fsanitize=thread)

# IMPORTANT: keep this source list in sync with the main project's audio test
# target (see tests/CMakeLists.txt). This does not automatically track the
# real target — if a file is added to or removed from the main audio test
# suite, update this list too, or the TSan build silently stops testing what
# the real suite tests.
add_executable(audio_tests_tsan
    ../tests/audio/ring_buffer_test.cpp
    ../tests/audio/audio_features_test.cpp
    ../tests/audio/feature_extractor_test.cpp
    ../tests/audio/file_producer_test.cpp
    ../tests/audio/feature_worker_test.cpp
    ../src/audio/ring_buffer.cpp
    ../src/audio/feature_extractor.cpp
    ../src/audio/file_producer.cpp
    ../src/audio/feature_worker.cpp
    ../src/audio/miniaudio_backend.cpp
)

target_include_directories(audio_tests_tsan PRIVATE
    ../src
    ../tests
)

include(FetchContent)

# miniaudio (header-only) — needed by file_producer.cpp/miniaudio_backend.cpp.
# Same tag as the root CMakeLists.txt.
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG        0.11.25
)
FetchContent_GetProperties(miniaudio)
if(NOT miniaudio_POPULATED)
    FetchContent_Populate(miniaudio)
endif()
add_library(miniaudio INTERFACE)
target_include_directories(miniaudio INTERFACE ${miniaudio_SOURCE_DIR})

# pffft — needed by feature_extractor.cpp. Same tag/config as the root
# CMakeLists.txt (float precision, SIMD on, no explicit arch opt-in).
set(PFFFT_USE_TYPE_FLOAT ON CACHE BOOL "" FORCE)
set(PFFFT_USE_TYPE_DOUBLE OFF CACHE BOOL "" FORCE)
set(PFFFT_USE_SIMD ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    pffft
    GIT_REPOSITORY https://github.com/marton78/pffft.git
    GIT_TAG        v1.1.0
)
FetchContent_MakeAvailable(pffft)

# GoogleTest/GoogleMock — same tag as the root CMakeLists.txt. A version
# mismatch between this build and the real one is exactly the kind of silent
# drift this file is meant to avoid.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
)
FetchContent_MakeAvailable(googletest)

target_link_libraries(audio_tests_tsan PRIVATE GTest::gtest_main GTest::gmock PFFFT miniaudio pthread)
