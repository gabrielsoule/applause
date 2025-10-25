#include "ExampleShowcasePlugin.h"
#include "applause/util/DebugHelpers.h"
#include "applause/util/Json.h"
#include <map>
#include <vector>
ExampleShowcasePlugin::ExampleShowcasePlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : PluginBase(descriptor, host),
      note_ports_(),
      params_(),
      gui_ext_([this]() { return std::make_unique<ExampleShowcaseEditor>(&params_); },
               800, 600)
{
    LOG_INFO("ExampleShowcase constructor");

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

    // Configure state extension callbacks - plugin manages its own versioning
    state_.setSaveCallback([this](applause::json& j)
    {
        // Save plugin state
        j["demo_value"] = 42;
        j["demo_string"] = "hello, Applause!";
        j["preset_values"] = std::vector<float>{0.1f, 0.5f, 0.75f, 1.0f};

        // Save parameters
        params_.saveToJson(j["parameters"]);

        return true;
    });

    state_.setLoadCallback([this](const applause::json& j)
    {
        int demo_value = j.value("demo_value", 0);
        std::string demo_string = j.value("demo_string", "default");
        
        if (j.contains("preset_values") && j["preset_values"].is_array()) {
            std::vector<float> preset_values = j["preset_values"];
        }
        
        if (j.contains("parameters")) {
            params_.loadFromJson(j["parameters"]);
        }

        LOG_INFO("Demo value: {}, string: {}", demo_value, demo_string);
        
        return true;
    });

    // Register extensions with the plugin
    registerExtension(note_ports_);
    registerExtension(audio_ports_);
    registerExtension(state_);
    registerExtension(params_);
    registerExtension(gui_ext_);
}

bool ExampleShowcasePlugin::init() noexcept {
    LOG_INFO("ExampleShowcase::init()");
    return true;
}

void ExampleShowcasePlugin::destroy() noexcept {
    LOG_INFO("ExampleShowcase::destroy()");
}

bool ExampleShowcasePlugin::activate(const applause::ProcessInfo& info) noexcept {
    LOG_INFO("ExampleShowcase::activate() - sampleRate: {}", info.sample_rate);
    return true;
}

void ExampleShowcasePlugin::deactivate() noexcept {
    LOG_INFO("ExampleShowcase::deactivate()");
}

clap_process_status ExampleShowcasePlugin::process(const clap_process_t* process) noexcept {
    
    // Let the parameter module process events.
    params_.processEvents(process->in_events, process->out_events);
    
    return CLAP_PROCESS_SLEEP;
}
