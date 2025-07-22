#pragma once

#include "core/Extension.h"
#include <clap/ext/note-ports.h>
#include <vector>
#include <string>

namespace applause {

/**
 * @brief This Applause extension lets plugins define MIDI input and output channels.
 */
class NotePortsExtension : public applause::IExtension {
private:
    /**
     * @brief Internal configuration for a single note port.
     */
    struct PortConfig {
        clap_id id;                   ///< Stable port identifier
        uint32_t supported_dialects;  ///< Bitfield of supported note dialects
        uint32_t preferred_dialect;   ///< Preferred dialect
        std::string name;             ///< Human-readable port name
        
        PortConfig(clap_id id, const std::string& name, uint32_t supported, uint32_t preferred)
            : id(id), supported_dialects(supported), preferred_dialect(preferred), name(name) {}
    };
    
    std::vector<PortConfig> input_ports_;   ///< Configured input ports
    std::vector<PortConfig> output_ports_;  ///< Configured output ports
    clap_id next_id_ = 0;                   ///< Auto-incrementing ID generator
    mutable clap_plugin_note_ports_t clap_struct_; ///< C struct for CLAP host
    
    /**
     * @brief CLAP C callback: Get number of note ports.
     * @param plugin Plugin instance 
     * @param is_input true for input ports, false for output ports
     * @return Number of ports of the specified direction
     */
    static uint32_t clap_note_ports_count(const clap_plugin_t* plugin, bool is_input) noexcept;
    
    /**
     * @brief CLAP C callback: Get information about a specific note port.
     * @param plugin Plugin instance
     * @param index Zero-based port index
     * @param is_input true for input ports, false for output ports  
     * @param info Output structure to fill with port information
     * @return true on success, false if index is invalid
     */
    static bool clap_note_ports_get(const clap_plugin_t* plugin, uint32_t index, bool is_input,
                            clap_note_port_info_t* info) noexcept;

    /**
     * @brief Helper to choose appropriate preferred dialect from supported dialects.
     * @param supported_dialects Bitfield of supported dialects
     * @return Single dialect value for preferred dialect
     */
    static uint32_t choose_preferred_dialect(uint32_t supported_dialects);

public:
    static constexpr const char* ID = CLAP_EXT_NOTE_PORTS;

    NotePortsExtension();

    const char* id() const override { return ID; }
    const void* getClapExtensionStruct() const override { return &clap_struct_; }

    /**
     * @brief Add an input port with single dialect (both preferred and supported).
     * @param name Display name for the port
     * @param dialect The note dialect to use (both preferred and supported)
     * @param customId Custom port ID (CLAP_INVALID_ID = auto-generate)
     * @return Reference to this extension for chaining
     */
    NotePortsExtension& addInput(const std::string& name, 
                                uint32_t dialect,
                                clap_id custom_id = CLAP_INVALID_ID);
    
    /**
     * @brief Add an output port with single dialect (both preferred and supported).
     * @param name Display name for the port
     * @param dialect The note dialect to use (both preferred and supported)
     * @param customId Custom port ID (CLAP_INVALID_ID = auto-generate)
     * @return Reference to this extension for chaining
     */
    NotePortsExtension& addOutput(const std::string& name,
                                 uint32_t dialect,
                                 clap_id custom_id = CLAP_INVALID_ID);
    
    /**
     * @brief Add an input port with separate supported and preferred dialects.
     * @param name Display name for the port
     * @param supportedDialects Bitfield of supported note dialects
     * @param preferredDialect Preferred dialect
     * @param customId Custom port ID (CLAP_INVALID_ID = auto-generate)
     * @return Reference to this extension for chaining
     */
    NotePortsExtension& addInput(const std::string& name, 
                                uint32_t supported_dialects,
                                uint32_t preferred_dialect,
                                clap_id custom_id = CLAP_INVALID_ID);
    
    /**
     * @brief Add an output port with separate supported and preferred dialects.
     * @param name Display name for the port
     * @param supportedDialects Bitfield of supported note dialects
     * @param preferredDialect Preferred dialect
     * @param customId Custom port ID (CLAP_INVALID_ID = auto-generate) 
     * @return Reference to this extension for chaining
     */
    NotePortsExtension& addOutput(const std::string& name,
                                 uint32_t supported_dialects,
                                 uint32_t preferred_dialect, 
                                 clap_id custom_id = CLAP_INVALID_ID);

    /**
     * @brief Get the number of configured input ports.
     * @return Number of input ports
     */
    size_t inputCount() const { return input_ports_.size(); }
    
    /**
     * @brief Get the number of configured output ports.
     * @return Number of output ports  
     */
    size_t outputCount() const { return output_ports_.size(); }
    
    /**
     * @brief Get read-only access to input port configurations.
     * @return Vector of input port configurations
     */
    const std::vector<PortConfig>& inputPorts() const { return input_ports_; }
    
    /**
     * @brief Get read-only access to output port configurations.
     * @return Vector of output port configurations
     */
    const std::vector<PortConfig>& outputPorts() const { return output_ports_; }
};

} // namespace applause