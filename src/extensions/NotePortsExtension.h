#pragma once

#include "../core/Extension.h"
#include <clap/ext/note-ports.h>
#include <vector>
#include <string>

namespace applause {

/**
 * @brief Configuration structure for a note port.
 * 
 * This struct uses C++20 designated initializers for clear, declarative configuration.
 * Common configurations can be created using the static helper functions.
 */
struct NotePortConfig {
    std::string name;                    ///< Display name for the port
    uint32_t supported_dialects;         ///< Bitfield of supported note dialects
    uint32_t preferred_dialect = 0;      ///< Preferred dialect (0 = auto-choose based on supported)
    clap_id id = CLAP_INVALID_ID;        ///< Port ID (CLAP_INVALID_ID = auto-generate)
    
    /**
     * @brief Create a MIDI port configuration.
     * @param name Display name for the port
     * @return Configuration for a standard MIDI port
     */
    static NotePortConfig midi(const std::string& name) {
        return {
            .name = name,
            .supported_dialects = CLAP_NOTE_DIALECT_MIDI,
            .preferred_dialect = CLAP_NOTE_DIALECT_MIDI
        };
    }
    
    /**
     * @brief Create a CLAP native port configuration.
     * @param name Display name for the port
     * @return Configuration for a CLAP native event port
     */
    static NotePortConfig clap(const std::string& name) {
        return {
            .name = name,
            .supported_dialects = CLAP_NOTE_DIALECT_CLAP,
            .preferred_dialect = CLAP_NOTE_DIALECT_CLAP
        };
    }
    
    /**
     * @brief Create a MIDI MPE port configuration.
     * @param name Display name for the port
     * @return Configuration for a MIDI MPE (Multidimensional Polyphonic Expression) port
     */
    static NotePortConfig midiMPE(const std::string& name) {
        return {
            .name = name,
            .supported_dialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_MIDI_MPE,
            .preferred_dialect = CLAP_NOTE_DIALECT_MIDI_MPE
        };
    }
    
    /**
     * @brief Create a universal port configuration supporting all dialects.
     * @param name Display name for the port
     * @return Configuration supporting CLAP, MIDI, MIDI2, and MPE
     */
    static NotePortConfig universal(const std::string& name) {
        return {
            .name = name,
            .supported_dialects = CLAP_NOTE_DIALECT_CLAP | CLAP_NOTE_DIALECT_MIDI | 
                                  CLAP_NOTE_DIALECT_MIDI_MPE | CLAP_NOTE_DIALECT_MIDI2,
            .preferred_dialect = CLAP_NOTE_DIALECT_CLAP
        };
    }
};

/**
 * @brief This Applause extension lets plugins define MIDI input and output channels.
 */
class NotePortsExtension : public IExtension {
private:
    /**
     * @brief Internal storage for a configured note port.
     */
    struct PortInfo {
        clap_id id;
        std::string name;
        uint32_t supported_dialects;
        uint32_t preferred_dialect;
        
        explicit PortInfo(const NotePortConfig& config, clap_id assigned_id)
            : id(assigned_id)
            , name(config.name)
            , supported_dialects(config.supported_dialects)
            , preferred_dialect(config.preferred_dialect) {}
    };
    
    std::vector<PortInfo> input_ports_;   ///< Configured input ports
    std::vector<PortInfo> output_ports_;  ///< Configured output ports
    clap_id next_id_ = 0;                 ///< Auto-incrementing ID generator
    
    // CLAP C callbacks
    static uint32_t clap_note_ports_count(const clap_plugin_t* plugin, bool is_input) noexcept;
    static bool clap_note_ports_get(const clap_plugin_t* plugin, uint32_t index, bool is_input,
                            clap_note_port_info_t* info) noexcept;

    // Helper to choose appropriate preferred dialect from supported dialects
    static uint32_t choose_preferred_dialect(uint32_t supported_dialects);
    
    // CLAP struct
    static constexpr clap_plugin_note_ports_t clap_struct_ = {
        .count = clap_note_ports_count,
        .get = clap_note_ports_get
    };

public:
    static constexpr const char* ID = CLAP_EXT_NOTE_PORTS;

    NotePortsExtension() = default;

    const char* id() const override { return ID; }
    const void* getClapExtensionStruct() const override { return &clap_struct_; }

    /**
     * @brief Add an input port using configuration struct.
     * @param config Port configuration
     * @return Reference to this extension for chaining
     */
    NotePortsExtension& addInput(const NotePortConfig& config) {
        clap_id id = (config.id == CLAP_INVALID_ID) ? next_id_++ : config.id;
        uint32_t preferred = config.preferred_dialect ? config.preferred_dialect 
                                                      : choose_preferred_dialect(config.supported_dialects);
        NotePortConfig adjusted_config = config;
        adjusted_config.preferred_dialect = preferred;
        adjusted_config.id = id;
        input_ports_.emplace_back(adjusted_config, id);
        return *this;
    }
    
    /**
     * @brief Add an output port using configuration struct.
     * @param config Port configuration
     * @return Reference to this extension for chaining
     */
    NotePortsExtension& addOutput(const NotePortConfig& config) {
        clap_id id = (config.id == CLAP_INVALID_ID) ? next_id_++ : config.id;
        uint32_t preferred = config.preferred_dialect ? config.preferred_dialect 
                                                      : choose_preferred_dialect(config.supported_dialects);
        NotePortConfig adjusted_config = config;
        adjusted_config.preferred_dialect = preferred;
        adjusted_config.id = id;
        output_ports_.emplace_back(adjusted_config, id);
        return *this;
    }

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
    const std::vector<PortInfo>& inputPorts() const { return input_ports_; }
    
    /**
     * @brief Get read-only access to output port configurations.
     * @return Vector of output port configurations
     */
    const std::vector<PortInfo>& outputPorts() const { return output_ports_; }
};

} // namespace applause