#pragma once

#include "core/PluginBase.h"
#include "extensions/AudioPortsExtension.h"
#include "util/DebugHelpers.h"

class ExampleNoiseGeneratorPlugin : public applause::PluginBase {
public:
    explicit ExampleNoiseGeneratorPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host);
    ~ExampleNoiseGeneratorPlugin() override = default;
    
    // Plugin lifecycle
    bool init() noexcept override;
    void destroy() noexcept override;
    bool activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept override;
    void deactivate() noexcept override;
    
    // Audio processing
    clap_process_status process(const clap_process_t* process) noexcept override;
    
private:
    // Extensions
    applause::AudioPortsExtension audio_ports_;
    
    // Fast PRNG state for white noise generation
    uint32_t noise_seed_;
};