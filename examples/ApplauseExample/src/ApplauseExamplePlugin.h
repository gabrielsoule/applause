#pragma once

#include "core/PluginBase.h"
#include "extensions/AudioPortsExtension.h"
#include "extensions/NotePortsExtension.h"
#include "extensions/StateExtension.h"
#include "extensions/ParamsExtension.h"
#include "extensions/GUIExtension.h"
#include "ApplauseExampleEditor.h"
#include "util/DebugHelpers.h"

class ApplauseExamplePlugin : public applause::PluginBase {
public:
    explicit ApplauseExamplePlugin(const clap_host_t* host);
    ~ApplauseExamplePlugin() override = default;
    
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
    applause::ParamsExtension params_;
    applause::GUIExtension<ApplauseExampleEditor> gui_ext_;
    
    // Parameter handles for efficient audio thread access
    applause::ParamHandle* param1_handle_ = nullptr;
    applause::ParamHandle* param2_handle_ = nullptr;
    applause::ParamHandle* filter_mode_handle_ = nullptr;
};