#pragma once

#include "../core/Extension.h"
#include <clap/ext/audio-ports.h>
#include <vector>
#include <string>

namespace applause {

/**
 * @brief Configuration structure for an audio port.
 * 
 * This struct uses C++20 designated initializers for clear, declarative configuration.
 * Common configurations can be created using the static helper functions.
 */
struct AudioPortConfig {
    std::string name;                       ///< Display name for the port
    uint32_t channel_count;                 ///< Number of audio channels
    std::string port_type = "";            ///< Port type (e.g., CLAP_PORT_STEREO, CLAP_PORT_MONO)
    uint32_t flags = 0;                     ///< Bitfield of CLAP_AUDIO_PORT_* flags
    clap_id in_place_pair = CLAP_INVALID_ID; ///< ID of paired port for in-place processing
    clap_id id = CLAP_INVALID_ID;           ///< Port ID (CLAP_INVALID_ID = auto-generate)
    
    /**
     * @brief Create a stereo port configuration.
     * @param name Display name for the port
     * @return Configuration for a stereo port
     */
    static AudioPortConfig stereo(const std::string& name) {
        return {
            .name = name,
            .channel_count = 2,
            .port_type = CLAP_PORT_STEREO
        };
    }
    
    /**
     * @brief Create a main stereo port configuration.
     * @param name Display name for the port
     * @return Configuration for a main stereo port
     */
    static AudioPortConfig mainStereo(const std::string& name) {
        return {
            .name = name,
            .channel_count = 2,
            .port_type = CLAP_PORT_STEREO,
            .flags = CLAP_AUDIO_PORT_IS_MAIN
        };
    }
    
    /**
     * @brief Create a mono port configuration.
     * @param name Display name for the port
     * @return Configuration for a mono port
     */
    static AudioPortConfig mono(const std::string& name) {
        return {
            .name = name,
            .channel_count = 1,
            .port_type = CLAP_PORT_MONO
        };
    }
    
    /**
     * @brief Create a main mono port configuration.
     * @param name Display name for the port
     * @return Configuration for a main mono port
     */
    static AudioPortConfig mainMono(const std::string& name) {
        return {
            .name = name,
            .channel_count = 1,
            .port_type = CLAP_PORT_MONO,
            .flags = CLAP_AUDIO_PORT_IS_MAIN
        };
    }
    
    /**
     * @brief Create a stereo port with in-place processing support.
     * @param name Display name for the port
     * @param pair_id ID of the paired port
     * @return Configuration for a stereo port with in-place processing
     */
    static AudioPortConfig stereoInPlace(const std::string& name, clap_id pair_id) {
        return {
            .name = name,
            .channel_count = 2,
            .port_type = CLAP_PORT_STEREO,
            .in_place_pair = pair_id
        };
    }
    
    /**
     * @brief Create a custom port configuration.
     * @param name Display name for the port
     * @param channels Number of channels
     * @param type Port type string (optional)
     * @return Configuration for a custom port
     */
    static AudioPortConfig custom(const std::string& name, uint32_t channels, const std::string& type = "") {
        return {
            .name = name,
            .channel_count = channels,
            .port_type = type
        };
    }
};

/**
 * @brief Extension for defining audio input and output ports in CLAP plugins.
 * 
 * This extension uses configuration structs for declaring audio ports with support
 * for common configurations (mono, stereo) and advanced features like 64-bit audio
 * and in-place processing.
 */
class AudioPortsExtension : public IExtension {
private:
    /**
     * @brief Internal storage for a configured audio port.
     */
    struct PortInfo {
        clap_id id;
        std::string name;
        uint32_t channel_count;
        std::string port_type;
        uint32_t flags;
        clap_id in_place_pair;
        
        explicit PortInfo(const AudioPortConfig& config, clap_id assigned_id)
            : id(assigned_id)
            , name(config.name)
            , channel_count(config.channel_count)
            , port_type(config.port_type)
            , flags(config.flags)
            , in_place_pair(config.in_place_pair) {}
    };
    
    std::vector<PortInfo> input_ports_;
    std::vector<PortInfo> output_ports_;
    clap_id next_id_ = 0;
    
    // CLAP C callbacks
    static uint32_t clap_audio_ports_count(const clap_plugin_t* plugin, bool is_input) noexcept;
    static bool clap_audio_ports_get(const clap_plugin_t* plugin, uint32_t index, bool is_input,
                                     clap_audio_port_info_t* info) noexcept;
    
    // CLAP struct
    static constexpr clap_plugin_audio_ports_t clap_struct_ = {
        .count = clap_audio_ports_count,
        .get = clap_audio_ports_get
    };
    
public:
    static constexpr const char* ID = CLAP_EXT_AUDIO_PORTS;
    
    AudioPortsExtension() = default;
    
    const char* id() const override { return ID; }
    const void* getClapExtensionStruct() const override { return &clap_struct_; }
    
    /**
     * @brief Add an input port using configuration struct.
     * @param config Port configuration
     * @return Reference to this extension for chaining
     */
    AudioPortsExtension& addInput(const AudioPortConfig& config) {
        clap_id id = (config.id == CLAP_INVALID_ID) ? next_id_++ : config.id;
        input_ports_.emplace_back(config, id);
        return *this;
    }
    
    /**
     * @brief Add an output port using configuration struct.
     * @param config Port configuration
     * @return Reference to this extension for chaining
     */
    AudioPortsExtension& addOutput(const AudioPortConfig& config) {
        clap_id id = (config.id == CLAP_INVALID_ID) ? next_id_++ : config.id;
        output_ports_.emplace_back(config, id);
        return *this;
    }
    
    /**
     * @brief Get the number of input ports.
     */
    size_t inputCount() const { return input_ports_.size(); }
    
    /**
     * @brief Get the number of output ports.
     */
    size_t outputCount() const { return output_ports_.size(); }
    
    /**
     * @brief Get read-only access to input port configurations.
     */
    const std::vector<PortInfo>& inputPorts() const { return input_ports_; }
    
    /**
     * @brief Get read-only access to output port configurations.
     */
    const std::vector<PortInfo>& outputPorts() const { return output_ports_; }
};

} // namespace applause