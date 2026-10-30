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
#include "compositor/visual_param.h"

namespace panel_message_apply {

namespace {

void push_layer_stack(Compositor& compositor, UIHost& ui_host) {
    nlohmann::json msg;
    msg["kind"] = "layer_stack";
    msg["state"] = nlohmann::json::parse(layer_preset::serialize(compositor.layers()));
    ui_host.send_to_panel(msg.dump());
}

void push_preset_list(UIHost& ui_host, const std::string& presets_dir) {
    nlohmann::json msg;
    msg["kind"] = "preset_list";
    msg["files"] = preset_file_io::list_preset_files(presets_dir);
    ui_host.send_to_panel(msg.dump());
}

void push_status(UIHost& ui_host, bool ok, const std::string& text) {
    nlohmann::json msg;
    msg["kind"] = "status";
    msg["ok"] = ok;
    msg["message"] = text;
    ui_host.send_to_panel(msg.dump());
}

void push_full_state(PanelApplyContext& ctx) {
    push_layer_stack(ctx.compositor, ctx.ui_host);
    push_preset_list(ctx.ui_host, ctx.presets_dir);
}

void apply_one(const RequestStateMessage&, PanelApplyContext& ctx) {
    push_full_state(ctx);
    push_window_state(ctx.ui_host, ctx.window_layout, ctx.audio_source);
}

void apply_one(const AddLayerMessage& msg, PanelApplyContext& ctx) {
    if (ctx.compositor.add_layer(msg.visual_type).has_value()) {
        compositor_math::reassign_layer_tints(ctx.compositor.layers());
    } else {
        push_status(ctx.ui_host, false, "layer cap reached (" + std::to_string(Compositor::kMaxLayers) + ")");
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const RemoveLayerMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        ctx.compositor.remove_layer(*index);
        compositor_math::reassign_layer_tints(ctx.compositor.layers());
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const DuplicateLayerMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        if (ctx.compositor.duplicate_layer(*index).has_value()) {
            compositor_math::reassign_layer_tints(ctx.compositor.layers());
        } else {
            push_status(ctx.ui_host, false, "layer cap reached (" + std::to_string(Compositor::kMaxLayers) + ")");
        }
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const MoveLayerMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        ctx.compositor.move_layer(*index, msg.to_index);
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const SetLayerOpacityMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        ctx.compositor.layers()[*index].opacity = compositor_math::clamp_opacity(msg.opacity);
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const SetLayerBlendModeMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        ctx.compositor.layers()[*index].blend_mode = msg.blend_mode;
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const SetLayerFreqBandMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        ctx.compositor.layers()[*index].freq_band = msg.freq_band;
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const SetLayerReactivitySourceMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        ctx.compositor.layers()[*index].reactivity_source = msg.reactivity_source;
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const SetLayerEnabledMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        ctx.compositor.layers()[*index].enabled = msg.enabled;
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const SetLayerSensitivityMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        ctx.compositor.layers()[*index].sensitivity = compositor_math::clamp_sensitivity(msg.sensitivity);
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const SetPaletteOverrideMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        ctx.compositor.layers()[*index].palette_override =
            msg.palette_override ? std::optional<Palette>(*msg.palette_override) : std::nullopt;
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const SetVisualParamMessage& msg, PanelApplyContext& ctx) {
    std::optional<std::size_t> index = ctx.compositor.index_of_layer(msg.layer_id);
    if (index.has_value()) {
        if (!visual_param::set(ctx.compositor.layers()[*index], msg.param_name, msg.value)) {
            push_status(ctx.ui_host, false,
                        "visual param \"" + msg.param_name +
                            "\" rejected: unknown name or wrong value type for this layer's visual type");
        }
    }
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const LoadPresetMessage& msg, PanelApplyContext& ctx) {
    const std::string path = preset_file_io::preset_path(ctx.presets_dir, msg.filename);
    preset_file_io::ReadResult read_result = preset_file_io::read_file(path);
    if (!read_result.success) {
        push_status(ctx.ui_host, false, "failed loading preset from " + msg.filename + ": " + read_result.error);
        push_layer_stack(ctx.compositor, ctx.ui_host);
        return;
    }

    std::optional<std::vector<Layer>> loaded = layer_preset::deserialize(read_result.contents);
    if (!loaded.has_value()) {
        push_status(ctx.ui_host, false, "preset " + msg.filename + " is malformed or invalid");
        push_layer_stack(ctx.compositor, ctx.ui_host);
        return;
    }

    if (!ctx.compositor.replace_layers_if_ids_valid(std::move(*loaded))) {
        push_status(ctx.ui_host, false, "preset " + msg.filename + " was rejected by the compositor");
        push_layer_stack(ctx.compositor, ctx.ui_host);
        return;
    }

    push_status(ctx.ui_host, true,
                "loaded " + std::to_string(ctx.compositor.layers().size()) + " layer(s) from " + msg.filename);
    push_layer_stack(ctx.compositor, ctx.ui_host);
}

void apply_one(const SavePresetMessage& msg, PanelApplyContext& ctx) {
    const std::string path = preset_file_io::preset_path(ctx.presets_dir, msg.filename);
    const std::string json_text = layer_preset::serialize(ctx.compositor.layers());
    preset_file_io::WriteResult result = preset_file_io::write_file(path, json_text);
    if (result.success) {
        push_status(ctx.ui_host, true,
                    "saved " + std::to_string(ctx.compositor.layers().size()) + " layer(s) to " + msg.filename);
    } else {
        push_status(ctx.ui_host, false, "failed saving preset to " + msg.filename + ": " + result.error);
    }
    push_full_state(ctx);
}

void apply_one(const SetDockedMessage& msg, PanelApplyContext& ctx) {
    ctx.window_layout.set_docked(msg.docked);
    push_window_state(ctx.ui_host, ctx.window_layout, ctx.audio_source);
}

void apply_one(const SetFullscreenMessage& msg, PanelApplyContext& ctx) {
    ctx.window_layout.set_fullscreen(msg.fullscreen);
    push_window_state(ctx.ui_host, ctx.window_layout, ctx.audio_source);
}

void apply_one(const SwitchSourceMessage&, PanelApplyContext& ctx) {
    AudioSourceController::SwitchResult result = ctx.audio_source.toggle();
    if (!result.success) {
        push_status(ctx.ui_host, false, "failed switching audio source: " + result.error);
    }
    push_window_state(ctx.ui_host, ctx.window_layout, ctx.audio_source);
}

void apply_one(const ImportFileMessage& msg, PanelApplyContext& ctx) {
    AudioSourceController::SwitchResult result = ctx.audio_source.import_file(msg.path);
    if (result.success) {
        push_status(ctx.ui_host, true, "playing " + msg.path);
    } else {
        push_status(ctx.ui_host, false, "failed importing audio file: " + result.error);
    }
    push_window_state(ctx.ui_host, ctx.window_layout, ctx.audio_source);
}

void apply_one(const EnableLoopbackMessage&, PanelApplyContext& ctx) {
    AudioSourceController::SwitchResult result = ctx.audio_source.enable_loopback();
    if (!result.success) {
        push_status(ctx.ui_host, false, "failed enabling loopback: " + result.error);
    }
    push_window_state(ctx.ui_host, ctx.window_layout, ctx.audio_source);
}

}

void apply(const PanelMessage& message, PanelApplyContext& ctx) {
    std::visit([&ctx](const auto& typed_message) { apply_one(typed_message, ctx); }, message);
}

void push_window_state(UIHost& ui_host, const WindowLayoutController& window_layout,
                        const AudioSourceController& audio_source) {
    nlohmann::json msg;
    msg["kind"] = "window_state";
    msg["docked"] = window_layout.mode() == WindowMode::Docked;
    msg["fullscreen"] = window_layout.is_fullscreen();
    const char* source = "none";
    if (audio_source.kind() == AudioSourceKind::File) {
        source = "file";
    } else if (audio_source.kind() == AudioSourceKind::Loopback) {
        source = "loopback";
    }
    msg["source"] = source;
    ui_host.send_to_panel(msg.dump());
}

}
