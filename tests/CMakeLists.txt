# Test targets are added here as milestones introduce testable logic
# (see TESTING.md — pure-logic pieces get GoogleTest coverage).
# tests/ mirrors src/ 1:1 per STRUCTURE.md; no source has landed yet,
# so there is nothing to register.
#
# Pattern for adding a test target once one exists, e.g. tests/audio:
#
#   add_executable(audio_tests audio/banding_test.cpp)
#   target_link_libraries(audio_tests PRIVATE GTest::gtest_main GTest::gmock)
#   gtest_discover_tests(audio_tests)
