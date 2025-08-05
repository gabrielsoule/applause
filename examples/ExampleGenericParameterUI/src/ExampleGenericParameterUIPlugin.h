#pragma once

#include "applause/core/PluginBase.h"
#include "applause/extensions/ParamsExtension.h"
#include "applause/extensions/StateExtension.h"
#include "applause/extensions/GUIExtension.h"
#include "applause/ui/GenericParameterUIEditor.h"

class ExampleGenericParameterUIPlugin : public applause::PluginBase
{
public:
    ExampleGenericParameterUIPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host);
    ~ExampleGenericParameterUIPlugin() override = default;

    // Plugin lifecycle
    bool init() noexcept override;
    void destroy() noexcept override;
    bool activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept override;
    void deactivate() noexcept override;
    void reset() noexcept override {}
    clap_process_status process(const clap_process_t* process) noexcept override;

private:
    // Extensions
    applause::ParamsExtension params_;
    applause::StateExtension state_;
    applause::GUIExtension gui_ext_;
};