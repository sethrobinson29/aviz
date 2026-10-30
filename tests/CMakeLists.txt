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
  audio/audio_file_validation_test.cpp
  ../src/audio/ring_buffer.cpp
  ../src/audio/feature_extractor.cpp
  ../src/audio/file_producer.cpp
  ../src/audio/loopback_producer.cpp
  ../src/audio/pipeline_format_converter.cpp
  ../src/audio/feature_worker.cpp
  ../src/audio/miniaudio_backend.cpp
  ../src/audio/audio_file_validation.cpp
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
  visuals/number_clamp_test.cpp
  visuals/palette_test.cpp
  visuals/waveform_visual_test.cpp
  visuals/spectrum_visual_test.cpp
  visuals/radial_visual_test.cpp
  visuals/particles_visual_test.cpp
  visuals/lissajous_visual_test.cpp
  visuals/flow_field_visual_test.cpp
  visuals/fractal_visual_test.cpp
  ../src/visuals/palette.cpp
  ../src/visuals/waveform/waveform_visual.cpp
  ../src/visuals/spectrum/spectrum_visual.cpp
  ../src/visuals/radial/radial_visual.cpp
  ../src/visuals/particles/particles_visual.cpp
  ../src/visuals/lissajous/lissajous_visual.cpp
  ../src/visuals/flow_field/flow_field_visual.cpp
  ../src/visuals/fractal/fractal_visual.cpp
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
  compositor/layer_preset_test.cpp
  compositor/preset_file_io_test.cpp
  compositor/visual_param_test.cpp
  ../src/compositor/compositor_math.cpp
  ../src/compositor/compositor.cpp
  ../src/compositor/layer.cpp
  ../src/compositor/layer_preset.cpp
  ../src/compositor/preset_file_io.cpp
  ../src/compositor/visual_param.cpp
  ../src/visuals/palette.cpp
  ../src/visuals/waveform/waveform_visual.cpp
  ../src/visuals/spectrum/spectrum_visual.cpp
  ../src/visuals/radial/radial_visual.cpp
  ../src/visuals/particles/particles_visual.cpp
  ../src/visuals/lissajous/lissajous_visual.cpp
  ../src/visuals/flow_field/flow_field_visual.cpp
  ../src/visuals/fractal/fractal_visual.cpp
)
target_include_directories(compositor_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(compositor_tests PRIVATE GTest::gtest_main GTest::gmock raylib nlohmann_json::nlohmann_json)
gtest_discover_tests(compositor_tests)

# Startup layer-generation logic (-n debug flag, milestone-4-debug-controls.md)
# plus milestone 8's docked/detached window layout and audio-source-switch
# orchestration. window_layout_config/window_mode_state are pure (no
# raylib/window dependency at all); window_layout_controller.cpp itself is
# NOT unit tested here -- it calls real raylib window/monitor functions
# (SetWindowPosition, ToggleFullscreen, ...) that are unsafe to call without
# a live window, so its actual effect is human-review territory per
# TESTING.md, same as visual/audio correctness. audio_source_controller_test.cpp
# uses real FileProducer/LoopbackProducer (synthetic WAV fixture, same
# pattern as file_producer_test.cpp; unconditional real-device LoopbackProducer
# construction, same precedent as loopback_producer_test.cpp), so this target
# needs the same producer/miniaudio sources audio_tests does.
add_executable(app_tests
  app/layer_generator_test.cpp
  app/window_layout_config_test.cpp
  app/window_mode_state_test.cpp
  app/audio_source_controller_test.cpp
  ../src/app/layer_generator.cpp
  ../src/app/window_layout_config.cpp
  ../src/app/window_mode_state.cpp
  ../src/app/audio_source_controller.cpp
  ../src/audio/ring_buffer.cpp
  ../src/audio/file_producer.cpp
  ../src/audio/loopback_producer.cpp
  ../src/audio/pipeline_format_converter.cpp
  ../src/audio/miniaudio_backend.cpp
  ../src/audio/audio_file_validation.cpp
)
target_include_directories(app_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(app_tests PRIVATE GTest::gtest_main GTest::gmock raylib miniaudio)
gtest_discover_tests(app_tests)

# C++-side state bridge logic only (panel_message parsing/validation, the
# dispatch queue, the queue-to-Compositor round trip, and panel_message_apply
# -- the full message-to-Compositor/preset_file_io application layer) -- the
# WebView2 window itself (src/ui_host/ui_host_webview2.cpp) and web/panel/
# are not covered here per STRUCTURE.md's note that those are human-review
# territory, same as visual/audio correctness. panel_message_apply_test.cpp
# and panel_dispatch_round_trip_test.cpp share a harness (ui_host/panel_apply_
# test_fixture.h) bundling a hand-rolled FakeUIHost, a real WindowLayoutController,
# and a real AudioSourceController (wrapping a ProducerHost over a NiceMock
# producer -- MockProducer, extracted to tests/audio/mock_producer.h) -- since
# PanelApplyContext now needs all of them, not just Compositor/UIHost. Tests
# only exercise the SAFE (already-in-that-state, no-op) transitions of
# WindowLayoutController's dock/fullscreen messages: the real transitions call
# actual raylib window/monitor functions that are unsafe without a live
# window, so those stay human-review territory per TESTING.md, same as
# window_layout_controller.cpp itself (see app_tests' comment). SwitchSourceMessage
# IS exercised for real -- it only touches audio (FileProducer/LoopbackProducer),
# not raylib, so this target links the same producer/miniaudio sources
# app_tests does. Links Compositor (GL-free here, same add_layer-without-
# resize() pattern as compositor_tests) to prove messages actually apply, not
# just that the queue holds them. preset_file_io/layer_preset are real, not
# mocked, so preset save/load tests touch the real presets/ directory
# relative to this binary's working directory, same convention as
# preset_file_io_test.cpp.
add_executable(ui_host_tests
  ui_host/panel_message_test.cpp
  ui_host/panel_dispatch_queue_test.cpp
  ui_host/panel_dispatch_round_trip_test.cpp
  ui_host/panel_message_apply_test.cpp
  ui_host/panel_navigation_policy_test.cpp
  ../src/ui_host/panel_message.cpp
  ../src/ui_host/panel_dispatch_queue.cpp
  ../src/ui_host/panel_message_apply.cpp
  ../src/ui_host/panel_navigation_policy.cpp
  ../src/app/audio_source_controller.cpp
  ../src/app/window_layout_config.cpp
  ../src/app/window_layout_controller.cpp
  ../src/app/window_mode_state.cpp
  ../src/audio/ring_buffer.cpp
  ../src/audio/file_producer.cpp
  ../src/audio/loopback_producer.cpp
  ../src/audio/pipeline_format_converter.cpp
  ../src/audio/miniaudio_backend.cpp
  ../src/audio/audio_file_validation.cpp
  ../src/compositor/compositor.cpp
  ../src/compositor/compositor_math.cpp
  ../src/compositor/layer.cpp
  ../src/compositor/layer_preset.cpp
  ../src/compositor/preset_file_io.cpp
  ../src/compositor/visual_param.cpp
  ../src/visuals/palette.cpp
  ../src/visuals/waveform/waveform_visual.cpp
  ../src/visuals/spectrum/spectrum_visual.cpp
  ../src/visuals/radial/radial_visual.cpp
  ../src/visuals/particles/particles_visual.cpp
  ../src/visuals/lissajous/lissajous_visual.cpp
  ../src/visuals/flow_field/flow_field_visual.cpp
  ../src/visuals/fractal/fractal_visual.cpp
)
target_include_directories(ui_host_tests PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests)
target_link_libraries(ui_host_tests PRIVATE GTest::gtest_main GTest::gmock raylib nlohmann_json::nlohmann_json miniaudio)
gtest_discover_tests(ui_host_tests)
