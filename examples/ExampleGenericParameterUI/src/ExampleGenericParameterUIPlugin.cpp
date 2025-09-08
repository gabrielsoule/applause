#include "ExampleGenericParameterUIPlugin.h"
#include "applause/util/DebugHelpers.h"
#include <nlohmann/json.hpp>

ExampleGenericParameterUIPlugin::ExampleGenericParameterUIPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : PluginBase(descriptor, host),
      params_(host),
      gui_ext_(host, 
               [this]() { return std::make_unique<applause::GenericParameterUIEditor>(&params_); },
               400, 600)
{
    LOG_INFO("ExampleGenericParameterUI constructor");

    // Register parameters using struct-based configuration (same as ExampleShowcase)
    params_.registerParam(applause::ParamConfig{
        .string_id = "param1",
        .name = "Parameter 1",
        .short_name = "Param 1",
        .min_value = 0.0f,
        .max_value = 1.0f,
        .default_value = 0.5f
    });

    params_.registerParam(applause::ParamConfig{
        .string_id = "param2",
        .name = "Parameter 2",
        .short_name = "Param 2",
        .unit = "Hz",
        .min_value = 10.0f,
        .max_value = 20000.0f,
        .default_value = 400.0f
    });

    params_.registerParam(applause::ParamConfig{
        .string_id = "filter_mode",
        .name = "Filter Mode",
        .short_name = "Mode",
        .min_value = 0.0f,
        .max_value = 5.0f,
        .default_value = 0.0f,
        .is_stepped = true
    });

    // Configure state extension callbacks for parameter persistence
    state_.setSaveCallback([this](nlohmann::json& j)
    {
        params_.saveToJson(j["parameters"]);
        return true;
    });

    state_.setLoadCallback([this](const nlohmann::json& j)
    {
        if (j.contains("parameters")) {
            params_.loadFromJson(j["parameters"]);
        }
        return true;
    });

    // Register extensions with the plugin
    registerExtension(state_);
    registerExtension(params_);
    registerExtension(gui_ext_);
}

bool ExampleGenericParameterUIPlugin::init() noexcept {
    LOG_INFO("ExampleGenericParameterUI::init()");
    return true;
}

void ExampleGenericParameterUIPlugin::destroy() noexcept {
    LOG_INFO("ExampleGenericParameterUI::destroy()");
    // Force cleanup of GUI extension's editor to avoid static destruction order issues
    // This ensures all UI components (including TextEditors) are destroyed before 
    // static destructors run
    if (gui_ext_.getEditor()) {
        gui_ext_.getEditor()->close();
    }
}

bool ExampleGenericParameterUIPlugin::activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept {
    LOG_INFO("ExampleGenericParameterUI::activate() - sampleRate: {}", sampleRate);
    return true;
}

void ExampleGenericParameterUIPlugin::deactivate() noexcept {
    LOG_INFO("ExampleGenericParameterUI::deactivate()");
}

clap_process_status ExampleGenericParameterUIPlugin::process(const clap_process_t* process) noexcept {
    // Let the parameter module process events.
    params_.processEvents(process->in_events, process->out_events);
    
    // This example doesn't process audio, just demonstrates the UI
    return CLAP_PROCESS_SLEEP;
}