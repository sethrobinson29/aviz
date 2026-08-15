# tests/ mirrors src/ 1:1 per STRUCTURE.md. Pure-logic pieces get GoogleTest
# coverage per TESTING.md; visual/audio/UI correctness is human-reviewed,
# not unit tested.

add_executable(audio_tests
  audio/ring_buffer_test.cpp
  audio/audio_features_test.cpp
  audio/feature_extractor_test.cpp
  audio/file_producer_test.cpp
  audio/feature_worker_test.cpp
  ../src/audio/ring_buffer.cpp
  ../src/audio/feature_extractor.cpp
  ../src/audio/file_producer.cpp
  ../src/audio/feature_worker.cpp
  ../src/audio/miniaudio_backend.cpp
)
target_include_directories(audio_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(audio_tests PRIVATE GTest::gtest_main GTest::gmock PFFFT miniaudio)
gtest_discover_tests(audio_tests)

# Visual modules' pure-logic math (screen-space mapping) only -- rendering
# itself is human-review territory per TESTING.md, not unit tested. These
# link raylib for its plain value types (Vector2, Rectangle) but never
# initialize a window/GL context, since the functions under test don't call
# into one.
add_executable(visuals_tests
  visuals/waveform_visual_test.cpp
  visuals/spectrum_visual_test.cpp
  ../src/visuals/waveform/waveform_visual.cpp
  ../src/visuals/spectrum/spectrum_visual.cpp
)
target_include_directories(visuals_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(visuals_tests PRIVATE GTest::gtest_main GTest::gmock raylib)
gtest_discover_tests(visuals_tests)
