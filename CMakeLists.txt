cmake_minimum_required(VERSION 3.24)
project(aviz CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
add_executable(aviz src/main.cpp)
target_link_libraries(aviz PRIVATE miniaudio raylib)

# --- tests ---------------------------------------------------------------------
add_subdirectory(tests)
