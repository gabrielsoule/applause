#pragma once

#include "applause/core/PluginBase.h"
#include "applause/extensions/AudioPortsExtension.h"
#include "applause/extensions/ParamsExtension.h"
#include "applause/extensions/StateExtension.h"
#include "applause/extensions/GUIExtension.h"
#include "applause/util/DebugHelpers.h"

#include <chowdsp_filters/chowdsp_filters.h>

class ExampleFilterPlugin : public applause::PluginBase {
public:
    explicit ExampleFilterPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host);
    ~ExampleFilterPlugin() override = default;
    
    // Plugin lifecycle
    bool init() noexcept override;
    void destroy() noexcept override;
    bool activate(double sample_rate, uint32_t min_frames, uint32_t max_frames) noexcept override;
    void deactivate() noexcept override;
    
    // Audio processing
    clap_process_status process(const clap_process_t* process) noexcept override;
    
private:
    // Extensions
    applause::AudioPortsExtension audio_ports_;
    applause::ParamsExtension params_;
    applause::StateExtension state_;
    applause::GUIExtension gui_ext_;
    
    // DSP state
    double sample_rate_ = 44100.0;
    
    // Parameter handles (cached for efficiency)
    applause::ParamHandle* param_cutoff_ = nullptr;
    applause::ParamHandle* param_res_ = nullptr;
    applause::ParamHandle* param_mode_;
    
    // Last known parameter values for dirty checking
    float last_cutoff_ = -1.0f;
    float last_resonance_ = -1.0f;
    float last_mode_ = -1.0f;

    // Stereo filter using chowdsp StateVariableFilter
    using FilterType = chowdsp::StateVariableFilter<float, chowdsp::StateVariableFilterType::MultiMode>;
    FilterType filter_;
};