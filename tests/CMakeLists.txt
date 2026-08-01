# tests/ mirrors src/ 1:1 per STRUCTURE.md. Pure-logic pieces get GoogleTest
# coverage per TESTING.md; visual/audio/UI correctness is human-reviewed,
# not unit tested.

add_executable(audio_tests
  audio/ring_buffer_test.cpp
  audio/audio_features_test.cpp
  ../src/audio/ring_buffer.cpp
)
target_include_directories(audio_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(audio_tests PRIVATE GTest::gtest_main GTest::gmock)
gtest_discover_tests(audio_tests)
