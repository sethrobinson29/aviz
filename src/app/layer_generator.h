#pragma once

#include <cstddef>
#include <optional>
#include <random>
#include <vector>

#include "compositor/layer.h"

struct GeneratedLayerSpec {
    VisualType visual_type;
    std::optional<std::size_t> freq_band;
    ReactivitySource reactivity_source;
};

std::vector<GeneratedLayerSpec> generate_layer_specs(std::size_t count, std::mt19937& rng);
