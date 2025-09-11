#pragma once

#include "applause/extensions/StateExtension.h"

#include "applause/core/PluginBase.h"
#include "applause/extensions/AudioPortsExtension.h"
#include "applause/extensions/NotePortsExtension.h"
#include "applause/util/DebugHelpers.h"

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
    applause::NotePortsExtension note_ports_;
    applause::AudioPortsExtension audio_ports_;
    applause::StateExtension state_;
    
    // Fast PRNG state for white noise generation
    uint32_t noise_seed_;
};
