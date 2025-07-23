#pragma once

#include "core/PluginBase.h"
#include "extensions/AudioPortsExtension.h"
#include "extensions/NotePortsExtension.h"
#include "extensions/StateExtension.h"
#include "util/DebugHelpers.h"

namespace applause_example {

class ApplauseExample : public applause::PluginBase {
public:
    explicit ApplauseExample(const clap_host_t* host);
    ~ApplauseExample() override = default;
    
    static const clap_plugin_descriptor_t* getDescriptor();
    static const clap_plugin_factory_t factory;
    
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
};

} // namespace applause_example