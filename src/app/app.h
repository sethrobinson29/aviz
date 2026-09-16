#pragma once

#include <optional>

int run_app(const char* audio_path, std::optional<int> generate_layer_count = std::nullopt);
