# tests/ mirrors src/ 1:1 per STRUCTURE.md. Pure-logic pieces get GoogleTest
# coverage per TESTING.md; visual/audio/UI correctness is human-reviewed,
# not unit tested.

add_executable(audio_tests
  audio/ring_buffer_test.cpp
  audio/audio_features_test.cpp
  audio/feature_extractor_test.cpp
  audio/file_producer_test.cpp
  audio/feature_worker_test.cpp
  audio/loopback_producer_test.cpp
  audio/producer_host_test.cpp
  audio/pipeline_format_test.cpp
  ../src/audio/ring_buffer.cpp
  ../src/audio/feature_extractor.cpp
  ../src/audio/file_producer.cpp
  ../src/audio/loopback_producer.cpp
  ../src/audio/pipeline_format_converter.cpp
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
  visuals/radial_visual_test.cpp
  visuals/particles_visual_test.cpp
  visuals/lissajous_visual_test.cpp
  visuals/flow_field_visual_test.cpp
  ../src/visuals/waveform/waveform_visual.cpp
  ../src/visuals/spectrum/spectrum_visual.cpp
  ../src/visuals/radial/radial_visual.cpp
  ../src/visuals/particles/particles_visual.cpp
  ../src/visuals/lissajous/lissajous_visual.cpp
  ../src/visuals/flow_field/flow_field_visual.cpp
)
target_include_directories(visuals_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(visuals_tests PRIVATE GTest::gtest_main GTest::gmock raylib)
gtest_discover_tests(visuals_tests)

# Compositor's GL-free logic: blend-mode-to-GL-factor mapping, opacity
# clamping, layer reorder index math (compositor_math), plus
# Compositor::add_layer/remove_layer/move_layer and make_visual, none of
# which touch RenderTexture2D/BeginTextureMode as long as resize() is never
# called. render()/resize() themselves stay human-review territory per
# TESTING.md. Links raylib for its plain constants/types only, never
# initializes a window/GL context.
add_executable(compositor_tests
  compositor/compositor_math_test.cpp
  compositor/compositor_test.cpp
  compositor/layer_test.cpp
  ../src/compositor/compositor_math.cpp
  ../src/compositor/compositor.cpp
  ../src/compositor/layer.cpp
  ../src/visuals/waveform/waveform_visual.cpp
  ../src/visuals/spectrum/spectrum_visual.cpp
  ../src/visuals/radial/radial_visual.cpp
  ../src/visuals/particles/particles_visual.cpp
  ../src/visuals/lissajous/lissajous_visual.cpp
  ../src/visuals/flow_field/flow_field_visual.cpp
)
target_include_directories(compositor_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(compositor_tests PRIVATE GTest::gtest_main GTest::gmock raylib)
gtest_discover_tests(compositor_tests)

# Startup layer-generation logic (-n debug flag, milestone-4-debug-controls.md).
# Pure logic over VisualType/ReactivitySource/freq_band combinations -- no
# window/GL context, same raylib-linked-but-uninitialized pattern as
# compositor_tests above.
add_executable(app_tests
  app/layer_generator_test.cpp
  ../src/app/layer_generator.cpp
)
target_include_directories(app_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(app_tests PRIVATE GTest::gtest_main GTest::gmock raylib)
gtest_discover_tests(app_tests)
