#include "NotePortsExtension.h"
#include "applause/core/PluginBase.h"
#include <cstring>
#include <string_view>

namespace applause {

// NotePortConfig static methods
NotePortConfig NotePortConfig::midi(std::string_view name) {
    return {
        .name = std::string(name),
        .supported_dialects = CLAP_NOTE_DIALECT_MIDI,
        .preferred_dialect = CLAP_NOTE_DIALECT_MIDI
    };
}

NotePortConfig NotePortConfig::clap(std::string_view name) {
    return {
        .name = std::string(name),
        .supported_dialects = CLAP_NOTE_DIALECT_CLAP,
        .preferred_dialect = CLAP_NOTE_DIALECT_CLAP
    };
}

NotePortConfig NotePortConfig::midiMPE(std::string_view name) {
    return {
        .name = std::string(name),
        .supported_dialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI_MPE,
        .preferred_dialect = CLAP_NOTE_DIALECT_MIDI_MPE
    };
}

NotePortConfig NotePortConfig::universal(std::string_view name) {
    return {
        .name = std::string(name),
        .supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI | 
                              CLAP_NOTE_DIALECT_MIDI_MPE | CLAP_NOTE_DIALECT_MIDI2,
        .preferred_dialect = CLAP_NOTE_DIALECT_CLAP
    };
}

// NotePortsExtension constructor
NotePortsExtension::NotePortsExtension(const clap_host_t* host) : host_(host) {
    if (host_) {
        host_note_ports_ = static_cast<const clap_host_note_ports_t*>(
            host_->get_extension(host_, CLAP_EXT_NOTE_PORTS)
        );
    }
}

// NotePortsExtension methods
NotePortsExtension& NotePortsExtension::addInput(const NotePortConfig& config) {
    clap_id id = (config.id == CLAP_INVALID_ID) ? next_id_++ : config.id;
    uint32_t preferred = config.preferred_dialect ? config.preferred_dialect 
                                                  : choose_preferred_dialect(config.supported_dialects);
    NotePortConfig adjusted_config = config;
    adjusted_config.preferred_dialect = preferred;
    adjusted_config.id = id;
    input_ports_.emplace_back(adjusted_config, id);
    return *this;
}

NotePortsExtension& NotePortsExtension::addOutput(const NotePortConfig& config) {
    clap_id id = (config.id == CLAP_INVALID_ID) ? next_id_++ : config.id;
    uint32_t preferred = config.preferred_dialect ? config.preferred_dialect 
                                                  : choose_preferred_dialect(config.supported_dialects);
    NotePortConfig adjusted_config = config;
    adjusted_config.preferred_dialect = preferred;
    adjusted_config.id = id;
    output_ports_.emplace_back(adjusted_config, id);
    return *this;
}

size_t NotePortsExtension::inputCount() const { 
    return input_ports_.size(); 
}

size_t NotePortsExtension::outputCount() const { 
    return output_ports_.size(); 
}

const std::vector<NotePortsExtension::PortInfo>& NotePortsExtension::inputPorts() const { 
    return input_ports_; 
}

const std::vector<NotePortsExtension::PortInfo>& NotePortsExtension::outputPorts() const { 
    return output_ports_; 
}

uint32_t NotePortsExtension::getHostSupportedDialects() const {
    if (host_note_ports_) {
        return host_note_ports_->supported_dialects(host_);
    }
    return 0; // Host doesn't support note ports extension
}

const char* NotePortsExtension::id() const { 
    return ID; 
}

const void* NotePortsExtension::getClapExtensionStruct() const { 
    return &clap_struct_; 
}

uint32_t NotePortsExtension::clap_note_ports_count(const clap_plugin_t* plugin, bool is_input) noexcept {
    auto* ext = PluginBase::findExtension<NotePortsExtension>(plugin);
    if (!ext) return 0;
    
    return static_cast<uint32_t>(is_input ? ext->input_ports_.size() : ext->output_ports_.size());
}

bool NotePortsExtension::clap_note_ports_get(const clap_plugin_t* plugin, uint32_t index, bool is_input,
                        clap_note_port_info_t* info) noexcept {
    auto* ext = PluginBase::findExtension<NotePortsExtension>(plugin);
    if (!ext || !info) return false;
    
    const auto& ports = is_input ? ext->input_ports_ : ext->output_ports_;
    if (index >= ports.size()) return false;
    
    const auto& port = ports[index];
    info->id = port.id;
    info->supported_dialects = port.supported_dialects;
    info->preferred_dialect = port.preferred_dialect;
    
    // Safe string copy with null termination
    strncpy(info->name, port.name.c_str(), CLAP_NAME_SIZE - 1);
    info->name[CLAP_NAME_SIZE - 1] = '\0';
    
    return true;
}

uint32_t NotePortsExtension::choose_preferred_dialect(uint32_t supported_dialects) {
    // Priority: CLAP > MIDI > MPE > MIDI2
    if (supported_dialects & CLAP_NOTE_DIALECT_CLAP) return CLAP_NOTE_DIALECT_CLAP;
    if (supported_dialects & CLAP_NOTE_DIALECT_MIDI) return CLAP_NOTE_DIALECT_MIDI;
    if (supported_dialects & CLAP_NOTE_DIALECT_MIDI_MPE) return CLAP_NOTE_DIALECT_MIDI_MPE;
    if (supported_dialects & CLAP_NOTE_DIALECT_MIDI2) return CLAP_NOTE_DIALECT_MIDI2;
    return CLAP_NOTE_DIALECT_MIDI; // fallback
}

} // namespace applause