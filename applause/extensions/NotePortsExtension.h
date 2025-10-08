#pragma once

#include <clap/ext/note-ports.h>

#include <string>
#include <string_view>
#include <vector>

#include "applause/core/Extension.h"

namespace applause {

/**
 * @brief Configuration structure for a note port.
 *
 */
struct NotePortConfig {
    std::string name;             ///< Display name for the port
    uint32_t supported_dialects;  ///< Bitfield of supported note dialects
    uint32_t preferred_dialect =
        0;  ///< Preferred dialect (0 = auto-choose based on supported)
    clap_id id =
        CLAP_INVALID_ID;  ///< Port ID (CLAP_INVALID_ID = auto-generate)

    /**
     * @brief Create a MIDI port configuration.
     * @param name Display name for the port
     * @return Configuration for a standard MIDI port
     */
    static NotePortConfig midi(std::string_view name);

    /**
     * @brief Create a CLAP native port configuration.
     * @param name Display name for the port
     * @return Configuration for a CLAP native event port
     */
    static NotePortConfig clap(std::string_view name);

    /**
     * @brief Create a MIDI MPE port configuration.
     * @param name Display name for the port
     * @return Configuration for a MIDI MPE (Multidimensional Polyphonic
     * Expression) port
     */
    static NotePortConfig midiMPE(std::string_view name);

    /**
     * @brief Create a universal port configuration supporting all dialects.
     * @param name Display name for the port
     * @return Configuration supporting CLAP, MIDI, MIDI2, and MPE
     */
    static NotePortConfig universal(std::string_view name);
};

/**
 * @brief This Applause extension lets plugins define MIDI input and output
 * channels.
 *
 * If your plugin consumes MIDI, i.e. is an instrument or MIDI effect, you must
 * use this extension to define MIDI input ports
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
            : id(assigned_id),
              name(config.name),
              supported_dialects(config.supported_dialects),
              preferred_dialect(config.preferred_dialect) {}
    };

    std::vector<PortInfo> input_ports_;   ///< Configured input ports
    std::vector<PortInfo> output_ports_;  ///< Configured output ports
    clap_id next_id_ = 0;                 ///< Auto-incrementing ID generator

    // Host connection
    const clap_host_t* host_ = nullptr;
    const clap_host_note_ports_t* host_note_ports_ = nullptr;

    // CLAP C callbacks
    static uint32_t clap_note_ports_count(const clap_plugin_t* plugin,
                                          bool is_input) noexcept;
    static bool clap_note_ports_get(const clap_plugin_t* plugin, uint32_t index,
                                    bool is_input,
                                    clap_note_port_info_t* info) noexcept;

    // Helper to choose appropriate preferred dialect from supported dialects
    static uint32_t choose_preferred_dialect(uint32_t supported_dialects);

    // CLAP struct
    static constexpr clap_plugin_note_ports_t clap_struct_ = {
        .count = clap_note_ports_count, .get = clap_note_ports_get};

public:
    static constexpr const char* ID = CLAP_EXT_NOTE_PORTS;

    /**
     * @brief Constructor with host connection for dialect querying.
     * @param host CLAP host interface (can be nullptr for basic functionality)
     */
    explicit NotePortsExtension(const clap_host_t* host = nullptr);

    const char* id() const override;
    const void* getClapExtensionStruct() const override;

    /**
     * @brief Add an input port using configuration struct.
     * @param config Port configuration
     * @return Reference to this extension for chaining
     */
    NotePortsExtension& addInput(const NotePortConfig& config);

    /**
     * @brief Add an output port using configuration struct.
     * @param config Port configuration
     * @return Reference to this extension for chaining
     */
    NotePortsExtension& addOutput(const NotePortConfig& config);

    /**
     * @brief Get the number of configured input ports.
     * @return Number of input ports
     */
    size_t inputCount() const;

    /**
     * @brief Get the number of configured output ports.
     * @return Number of output ports
     */
    size_t outputCount() const;

    /**
     * @brief Get read-only access to input port configurations.
     * @return Vector of input port configurations
     */
    const std::vector<PortInfo>& inputPorts() const;

    /**
     * @brief Get read-only access to output port configurations.
     * @return Vector of output port configurations
     */
    const std::vector<PortInfo>& outputPorts() const;

    /**
     * @brief Query which note dialects the host supports.
     * @return Bitfield of supported dialects, or 0 if host doesn't support note
     * ports extension To use it, check individual dialects with bitwise AND
     * (&): bool supports_clap = (result & CLAP_NOTE_DIALECT_CLAP) != 0; bool
     * supports_mpe = (result & CLAP_NOTE_DIALECT_MIDI_MPE) != 0;
     *
     * Author's note: This hook is a little unreliable, and you probably shouldn't count on it. The VST3/AU wrapper
     * doesn't seem to pass anything through (which will look, to your synth, like no dialects are supported at all),
     * and I see inconsistent reporting even from a CLAP host in a CLAP plugin.
     *
     */
    uint32_t getHostSupportedDialects() const;
};

}  // namespace applause