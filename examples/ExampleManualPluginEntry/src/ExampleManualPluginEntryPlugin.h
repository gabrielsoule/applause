#pragma once

#include "core/PluginBase.h"
#include "extensions/AudioPortsExtension.h"
#include "extensions/NotePortsExtension.h"
#include "extensions/StateExtension.h"
#include "extensions/ParamsExtension.h"
#include "extensions/GUIExtension.h"
#include "util/DebugHelpers.h"

class ExampleManualPluginEntryPlugin : public applause::PluginBase {
public:
     explicit ExampleManualPluginEntryPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host);
    ~ExampleManualPluginEntryPlugin() override = default;
    
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
    applause::ParamsExtension params_;
    applause::GUIExtension gui_ext_;
};