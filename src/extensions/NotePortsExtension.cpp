#include "NotePortsExtension.h"
#include "../core/PluginBase.h"
#include <cstring>

namespace applause {

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