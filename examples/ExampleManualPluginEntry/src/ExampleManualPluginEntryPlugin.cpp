#include "ExampleManualPluginEntryPlugin.h"
#include <cstring>
#include <ExampleManualPluginEntryEditor.h>
#include "applause/util/DebugHelpers.h"
#include "applause/ui/ApplauseEditor.h"
#include <nlohmann/json.hpp>

ExampleManualPluginEntryPlugin::ExampleManualPluginEntryPlugin(const clap_plugin_descriptor_t* descriptor,
                                                               const clap_host_t* host)
    : PluginBase(descriptor, host),
      params_(host),
      gui_ext_(host,
               [this]() { return std::make_unique<ExampleManualPluginEntryEditor>(&params_); },
               800, 600)
{
    LOG_INFO("ExampleManualPluginEntry constructor");

    // Configure extensions
    note_ports_.addInput(applause::NotePortConfig::midi("MIDI In"));

    // It's also possible to use a QOL shortcut:
    // audio_ports_.addOutput(applause::AudioPortConfig::mainStereo("Main Out"));
    // But, in this example, we spell it all out in the interest of good pedagogy.
    audio_ports_.addOutput(applause::AudioPortConfig{
        .name = "Main Out",
        .channel_count = 2,
        .port_type = CLAP_PORT_STEREO,
        .flags = CLAP_AUDIO_PORT_IS_MAIN
    });

    // Register parameters using struct-based configuration
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
    registerExtension(note_ports_);
    registerExtension(audio_ports_);
    registerExtension(state_);
    registerExtension(params_);
    registerExtension(gui_ext_);
}

bool ExampleManualPluginEntryPlugin::init() noexcept
{
    LOG_INFO("ExampleManualPluginEntry::init()");
    return true;
}

void ExampleManualPluginEntryPlugin::destroy() noexcept
{
    LOG_INFO("ExampleManualPluginEntry::destroy()");
}

bool ExampleManualPluginEntryPlugin::activate(const applause::ProcessInfo& info) noexcept
{
    LOG_INFO("ExampleManualPluginEntry::activate() - sampleRate: {}", info.sample_rate);
    return true;
}

void ExampleManualPluginEntryPlugin::deactivate() noexcept
{
    LOG_INFO("ExampleManualPluginEntry::deactivate()");
}

clap_process_status ExampleManualPluginEntryPlugin::process(const clap_process_t* process) noexcept
{
    // Let the parameter module process events.
    params_.processEvents(process->in_events, process->out_events);

    return CLAP_PROCESS_SLEEP;
}
