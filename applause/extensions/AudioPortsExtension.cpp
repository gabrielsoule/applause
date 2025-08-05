#include "AudioPortsExtension.h"
#include "applause/core/PluginBase.h"
#include <cstring>

namespace applause {

uint32_t AudioPortsExtension::clap_audio_ports_count(const clap_plugin_t* plugin, bool is_input) noexcept {
    auto* ext = PluginBase::findExtension<AudioPortsExtension>(plugin);
    if (!ext) return 0;
    
    return static_cast<uint32_t>(is_input ? ext->input_ports_.size() : ext->output_ports_.size());
}

bool AudioPortsExtension::clap_audio_ports_get(const clap_plugin_t* plugin, uint32_t index, bool is_input,
                                                clap_audio_port_info_t* info) noexcept {
    auto* ext = PluginBase::findExtension<AudioPortsExtension>(plugin);
    if (!ext || !info) return false;
    
    const auto& ports = is_input ? ext->input_ports_ : ext->output_ports_;
    if (index >= ports.size()) return false;
    
    const auto& port = ports[index];
    
    // Fill in the info structure
    info->id = port.id;
    info->flags = port.flags;
    info->channel_count = port.channel_count;
    info->in_place_pair = port.in_place_pair;
    
    // Safe string copy for name
    strncpy(info->name, port.name.c_str(), CLAP_NAME_SIZE - 1);
    info->name[CLAP_NAME_SIZE - 1] = '\0';
    
    // Set port type - convert std::string to const char*
    // Note: This is safe because CLAP expects port_type to point to a static string
    // (like CLAP_PORT_STEREO) or null. We store common types as std::string but
    // they match the CLAP constants.
    if (port.port_type.empty()) {
        info->port_type = nullptr;
    } else if (port.port_type == CLAP_PORT_STEREO) {
        info->port_type = CLAP_PORT_STEREO;
    } else if (port.port_type == CLAP_PORT_MONO) {
        info->port_type = CLAP_PORT_MONO;
    } else {
        // For custom port types, we need to ensure the string remains valid
        // In a real implementation, you might want to store these as static strings
        info->port_type = port.port_type.c_str();
    }
    
    return true;
}

} // namespace applause
