#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "compositor/layer_types.h"
#include "visuals/reactivity_source.h"

struct RequestStateMessage {};

struct AddLayerMessage {
    VisualType visual_type;
};

struct RemoveLayerMessage {
    std::uint64_t layer_id;
};

struct DuplicateLayerMessage {
    std::uint64_t layer_id;
};

struct MoveLayerMessage {
    std::uint64_t layer_id;
    std::size_t to_index;
};

struct SetLayerOpacityMessage {
    std::uint64_t layer_id;
    float opacity;
};

struct SetLayerBlendModeMessage {
    std::uint64_t layer_id;
    LayerBlendMode blend_mode;
};

struct SetLayerFreqBandMessage {
    std::uint64_t layer_id;
    std::optional<std::size_t> freq_band;
};

struct SetLayerReactivitySourceMessage {
    std::uint64_t layer_id;
    ReactivitySource reactivity_source;
};

struct SetLayerEnabledMessage {
    std::uint64_t layer_id;
    bool enabled;
};

struct SetLayerSensitivityMessage {
    std::uint64_t layer_id;
    float sensitivity;
};

struct LoadPresetMessage {
    std::string filename;
};

struct SavePresetMessage {
    std::string filename;
};

using PanelMessage =
    std::variant<RequestStateMessage, AddLayerMessage, RemoveLayerMessage, DuplicateLayerMessage,
                 MoveLayerMessage, SetLayerOpacityMessage, SetLayerBlendModeMessage, SetLayerFreqBandMessage,
                 SetLayerReactivitySourceMessage, SetLayerEnabledMessage, SetLayerSensitivityMessage,
                 LoadPresetMessage, SavePresetMessage>;

namespace panel_message {

std::optional<PanelMessage> parse(std::string_view json_text);

}
