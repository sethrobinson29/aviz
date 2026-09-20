#include "ui_host/panel_message_apply.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

#include "compositor/compositor_math.h"
#include "compositor/layer.h"
#include "compositor/layer_preset.h"
#include "compositor/preset_file_io.h"

namespace panel_message_apply {

namespace {

void push_layer_stack(Compositor& compositor, UIHost& ui_host) {
    nlohmann::json msg;
    msg["kind"] = "layer_stack";
    msg["state"] = nlohmann::json::parse(layer_preset::serialize(compositor.layers()));
    ui_host.send_to_panel(msg.dump());
}

void push_preset_list(UIHost& ui_host) {
    nlohmann::json msg;
    msg["kind"] = "preset_list";
    msg["files"] = preset_file_io::list_preset_files();
    ui_host.send_to_panel(msg.dump());
}

void push_status(UIHost& ui_host, bool ok, const std::string& text) {
    nlohmann::json msg;
    msg["kind"] = "status";
    msg["ok"] = ok;
    msg["message"] = text;
    ui_host.send_to_panel(msg.dump());
}

void push_full_state(Compositor& compositor, UIHost& ui_host) {
    push_layer_stack(compositor, ui_host);
    push_preset_list(ui_host);
}

void apply_one(const RequestStateMessage&, Compositor& compositor, UIHost& ui_host) {
    push_full_state(compositor, ui_host);
}

void apply_one(const AddLayerMessage& msg, Compositor& compositor, UIHost& ui_host) {
    if (compositor.add_layer(msg.visual_type).has_value()) {
        compositor_math::reassign_layer_tints(compositor.layers());
    } else {
        push_status(ui_host, false, "layer cap reached (" + std::to_string(Compositor::kMaxLayers) + ")");
    }
    push_layer_stack(compositor, ui_host);
}

void apply_one(const RemoveLayerMessage& msg, Compositor& compositor, UIHost& ui_host) {
    std::optional<std::size_t> index = compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        compositor.remove_layer(*index);
        compositor_math::reassign_layer_tints(compositor.layers());
    }
    push_layer_stack(compositor, ui_host);
}

void apply_one(const DuplicateLayerMessage& msg, Compositor& compositor, UIHost& ui_host) {
    std::optional<std::size_t> index = compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        if (compositor.duplicate_layer(*index).has_value()) {
            compositor_math::reassign_layer_tints(compositor.layers());
        } else {
            push_status(ui_host, false, "layer cap reached (" + std::to_string(Compositor::kMaxLayers) + ")");
        }
    }
    push_layer_stack(compositor, ui_host);
}

void apply_one(const MoveLayerMessage& msg, Compositor& compositor, UIHost& ui_host) {
    std::optional<std::size_t> index = compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        compositor.move_layer(*index, msg.to_index);
    }
    push_layer_stack(compositor, ui_host);
}

void apply_one(const SetLayerOpacityMessage& msg, Compositor& compositor, UIHost& ui_host) {
    std::optional<std::size_t> index = compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        compositor.layers()[*index].opacity = compositor_math::clamp_opacity(msg.opacity);
    }
    push_layer_stack(compositor, ui_host);
}

void apply_one(const SetLayerBlendModeMessage& msg, Compositor& compositor, UIHost& ui_host) {
    std::optional<std::size_t> index = compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        compositor.layers()[*index].blend_mode = msg.blend_mode;
    }
    push_layer_stack(compositor, ui_host);
}

void apply_one(const SetLayerFreqBandMessage& msg, Compositor& compositor, UIHost& ui_host) {
    std::optional<std::size_t> index = compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        compositor.layers()[*index].freq_band = msg.freq_band;
    }
    push_layer_stack(compositor, ui_host);
}

void apply_one(const SetLayerReactivitySourceMessage& msg, Compositor& compositor, UIHost& ui_host) {
    std::optional<std::size_t> index = compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        compositor.layers()[*index].reactivity_source = msg.reactivity_source;
    }
    push_layer_stack(compositor, ui_host);
}

void apply_one(const SetLayerEnabledMessage& msg, Compositor& compositor, UIHost& ui_host) {
    std::optional<std::size_t> index = compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        compositor.layers()[*index].enabled = msg.enabled;
    }
    push_layer_stack(compositor, ui_host);
}

void apply_one(const SetLayerSensitivityMessage& msg, Compositor& compositor, UIHost& ui_host) {
    std::optional<std::size_t> index = compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        compositor.layers()[*index].sensitivity = compositor_math::clamp_sensitivity(msg.sensitivity);
    }
    push_layer_stack(compositor, ui_host);
}

void apply_one(const LoadPresetMessage& msg, Compositor& compositor, UIHost& ui_host) {
    const std::string path = preset_file_io::preset_path(msg.filename);
    preset_file_io::ReadResult read_result = preset_file_io::read_file(path);
    if (!read_result.success) {
        push_status(ui_host, false, "failed loading preset from " + msg.filename + ": " + read_result.error);
        push_layer_stack(compositor, ui_host);
        return;
    }

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(read_result.contents);
    if (!loaded.has_value()) {
        push_status(ui_host, false, "preset " + msg.filename + " is malformed or invalid");
        push_layer_stack(compositor, ui_host);
        return;
    }

    if (!compositor.replace_layers_if_ids_valid(std::move(*loaded))) {
        push_status(ui_host, false, "preset " + msg.filename + " was rejected by the compositor");
        push_layer_stack(compositor, ui_host);
        return;
    }

    push_status(ui_host, true,
                "loaded " + std::to_string(compositor.layers().size()) + " layer(s) from " + msg.filename);
    push_layer_stack(compositor, ui_host);
}

void apply_one(const SavePresetMessage& msg, Compositor& compositor, UIHost& ui_host) {
    const std::string path = preset_file_io::preset_path(msg.filename);
    const std::string json_text = layer_preset::serialize(compositor.layers());
    preset_file_io::WriteResult result = preset_file_io::write_file(path, json_text);
    if (result.success) {
        push_status(ui_host, true,
                    "saved " + std::to_string(compositor.layers().size()) + " layer(s) to " + msg.filename);
    } else {
        push_status(ui_host, false, "failed saving preset to " + msg.filename + ": " + result.error);
    }
    push_full_state(compositor, ui_host);
}

}

void apply(const PanelMessage& message, Compositor& compositor, UIHost& ui_host) {
    std::visit([&compositor, &ui_host](const auto& typed_message) { apply_one(typed_message, compositor, ui_host); },
               message);
}

}
