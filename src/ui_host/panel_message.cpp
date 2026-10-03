#include "ui_host/panel_message.h"

#include <array>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "audio/audio_config.h"
#include "audio/audio_file_validation.h"
#include "compositor/preset_file_io.h"

namespace {

template <typename Enum>
using EnumEntry = std::pair<Enum, const char*>;

constexpr std::array<EnumEntry<VisualType>, 7> kVisualTypeTable{{
    {VisualType::Waveform, "Waveform"},
    {VisualType::Spectrum, "Spectrum"},
    {VisualType::Radial, "Radial"},
    {VisualType::Particles, "Particles"},
    {VisualType::Lissajous, "Lissajous"},
    {VisualType::FlowField, "FlowField"},
    {VisualType::Fractal, "Fractal"},
}};

constexpr std::array<EnumEntry<LayerBlendMode>, 4> kBlendModeTable{{
    {LayerBlendMode::Normal, "Normal"},
    {LayerBlendMode::Additive, "Additive"},
    {LayerBlendMode::Multiply, "Multiply"},
    {LayerBlendMode::Screen, "Screen"},
}};

constexpr std::array<EnumEntry<ReactivitySource>, 3> kReactivitySourceTable{{
    {ReactivitySource::GlobalBeat, "GlobalBeat"},
    {ReactivitySource::BandOnset, "BandOnset"},
    {ReactivitySource::BandAmplitude, "BandAmplitude"},
}};

template <typename Enum, std::size_t N>
Enum string_to_enum(const std::string& text, const std::array<EnumEntry<Enum>, N>& table) {
    for (const auto& entry : table) {
        if (text == entry.second) {
            return entry.first;
        }
    }
    throw std::invalid_argument("unknown enum string \"" + text + "\"");
}

std::string require_string(const nlohmann::json& j, const char* key) {
    const nlohmann::json& field = j.at(key);
    if (!field.is_string()) {
        throw std::invalid_argument(std::string(key) + " must be a string");
    }
    return field.get<std::string>();
}

bool require_bool(const nlohmann::json& j, const char* key) {
    const nlohmann::json& field = j.at(key);
    if (!field.is_boolean()) {
        throw std::invalid_argument(std::string(key) + " must be a boolean");
    }
    return field.get<bool>();
}

float require_float(const nlohmann::json& j, const char* key) {
    const nlohmann::json& field = j.at(key);
    if (!field.is_number()) {
        throw std::invalid_argument(std::string(key) + " must be a number");
    }
    return field.get<float>();
}

VisualParamValue require_visual_param_value(const nlohmann::json& j) {
    const nlohmann::json& field = j.at("value");
    if (field.is_boolean()) {
        return field.get<bool>();
    }
    if (field.is_number()) {
        return field.get<float>();
    }
    throw std::invalid_argument("value must be a number or a boolean");
}

std::uint64_t require_layer_id(const nlohmann::json& j) {
    const nlohmann::json& field = j.at("layer_id");
    if (!field.is_number_unsigned()) {
        throw std::invalid_argument("layer_id must be a non-negative integer");
    }
    return field.get<std::uint64_t>();
}

std::size_t require_index(const nlohmann::json& j, const char* key) {
    const nlohmann::json& field = j.at(key);
    if (!field.is_number_unsigned()) {
        throw std::invalid_argument(std::string(key) + " must be a non-negative integer");
    }
    return static_cast<std::size_t>(field.get<std::uint64_t>());
}

std::string require_preset_filename(const nlohmann::json& j) {
    const std::string filename = require_string(j, "filename");
    if (!preset_file_io::is_safe_preset_filename(filename)) {
        throw std::invalid_argument("filename is not a safe preset filename");
    }
    return filename;
}

std::string require_param_name(const nlohmann::json& j) {
    const std::string name = require_string(j, "param_name");
    if (name.empty()) {
        throw std::invalid_argument("param_name must not be empty");
    }
    for (unsigned char c : name) {
        if (!std::isalnum(c) && c != '_') {
            throw std::invalid_argument("param_name contains an unexpected character");
        }
    }
    return name;
}

std::string require_audio_file_path(const nlohmann::json& j) {
    const std::string path = require_string(j, "path");
    if (!audio_file_validation::is_plausible_audio_file_path(path)) {
        throw std::invalid_argument("path is not a plausible audio file path");
    }
    return path;
}

std::optional<std::size_t> parse_freq_band(const nlohmann::json& j) {
    const nlohmann::json& field = j.at("freq_band");
    if (field.is_null()) {
        return std::nullopt;
    }
    if (!field.is_number_unsigned()) {
        throw std::invalid_argument("freq_band must be null or a non-negative integer");
    }
    const std::uint64_t band = field.get<std::uint64_t>();
    if (band >= audio_config::kNumBands) {
        throw std::invalid_argument("freq_band out of range");
    }
    return static_cast<std::size_t>(band);
}

PanelMessage parse_typed(const std::string& type, const nlohmann::json& doc) {
    if (type == "request_state") {
        return RequestStateMessage{};
    }
    if (type == "add_layer") {
        return AddLayerMessage{string_to_enum<VisualType>(require_string(doc, "visual_type"), kVisualTypeTable)};
    }
    if (type == "remove_layer") {
        return RemoveLayerMessage{require_layer_id(doc)};
    }
    if (type == "duplicate_layer") {
        return DuplicateLayerMessage{require_layer_id(doc)};
    }
    if (type == "move_layer") {
        return MoveLayerMessage{require_layer_id(doc), require_index(doc, "to_index")};
    }
    if (type == "set_opacity") {
        return SetLayerOpacityMessage{require_layer_id(doc), require_float(doc, "opacity")};
    }
    if (type == "set_blend_mode") {
        return SetLayerBlendModeMessage{
            require_layer_id(doc), string_to_enum<LayerBlendMode>(require_string(doc, "blend_mode"), kBlendModeTable)};
    }
    if (type == "set_freq_band") {
        return SetLayerFreqBandMessage{require_layer_id(doc), parse_freq_band(doc)};
    }
    if (type == "set_reactivity_source") {
        return SetLayerReactivitySourceMessage{
            require_layer_id(doc),
            string_to_enum<ReactivitySource>(require_string(doc, "reactivity_source"), kReactivitySourceTable)};
    }
    if (type == "set_enabled") {
        return SetLayerEnabledMessage{require_layer_id(doc), require_bool(doc, "enabled")};
    }
    if (type == "set_sensitivity") {
        return SetLayerSensitivityMessage{require_layer_id(doc), require_float(doc, "sensitivity")};
    }
    if (type == "set_visual_param") {
        return SetVisualParamMessage{require_layer_id(doc), require_param_name(doc),
                                     require_visual_param_value(doc)};
    }
    if (type == "load_preset") {
        return LoadPresetMessage{require_preset_filename(doc)};
    }
    if (type == "save_preset") {
        return SavePresetMessage{require_preset_filename(doc)};
    }
    if (type == "set_docked") {
        return SetDockedMessage{require_bool(doc, "docked")};
    }
    if (type == "set_fullscreen") {
        return SetFullscreenMessage{require_bool(doc, "fullscreen")};
    }
    if (type == "switch_source") {
        return SwitchSourceMessage{};
    }
    if (type == "import_file") {
        return ImportFileMessage{require_audio_file_path(doc)};
    }
    if (type == "enable_loopback") {
        return EnableLoopbackMessage{};
    }
    throw std::invalid_argument("unknown message type \"" + type + "\"");
}

}

namespace panel_message {

std::optional<PanelMessage> parse(std::string_view json_text) {
    try {
        const nlohmann::json doc = nlohmann::json::parse(json_text);
        if (!doc.is_object()) {
            return std::nullopt;
        }
        const std::string type = require_string(doc, "type");
        return parse_typed(type, doc);
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    }
}

}
