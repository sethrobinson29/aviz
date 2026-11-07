#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "compositor/layer_types.h"
#include "compositor/visual_param_value.h"
#include "visuals/reactivity_source.h"

struct Palette;

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

struct SetPaletteOverrideMessage {
    std::uint64_t layer_id;
    std::shared_ptr<const Palette> palette_override;
};

struct SetLayerNameMessage {
    std::uint64_t layer_id;
    std::string name;
};

struct SetVisualParamMessage {
    std::uint64_t layer_id;
    std::string param_name;
    VisualParamValue value;
};

struct LoadPresetFileMessage {
    std::string path;
};

struct SavePresetMessage {
    std::string filename;
};

struct SetDockedMessage {
    bool docked;
};

struct SetFullscreenMessage {
    bool fullscreen;
};

struct SwitchSourceMessage {};

struct ImportFileMessage {
    std::string path;
};

struct EnableLoopbackMessage {};

using PanelMessage =
    std::variant<RequestStateMessage, AddLayerMessage, RemoveLayerMessage, DuplicateLayerMessage,
                 MoveLayerMessage, SetLayerOpacityMessage, SetLayerBlendModeMessage, SetLayerFreqBandMessage,
                 SetLayerReactivitySourceMessage, SetLayerEnabledMessage, SetLayerSensitivityMessage,
                 SetPaletteOverrideMessage, SetLayerNameMessage, SetVisualParamMessage, LoadPresetFileMessage,
                 SavePresetMessage, SetDockedMessage, SetFullscreenMessage, SwitchSourceMessage,
                 ImportFileMessage, EnableLoopbackMessage>;

namespace panel_message {

std::optional<PanelMessage> parse(std::string_view json_text);

}
