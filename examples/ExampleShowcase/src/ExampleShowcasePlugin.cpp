#include "ExampleShowcasePlugin.h"
#include "applause/util/DebugHelpers.h"
#include <map>
#include <vector>
ExampleShowcasePlugin::ExampleShowcasePlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : PluginBase(descriptor, host),
      note_ports_(host),
      params_(host),
      gui_ext_(host, 
               [this]() { return std::make_unique<ExampleShowcaseEditor>(&params_); },
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

    // Configure state extension callbacks for parameter persistence
    state_.setSaveCallback([this](auto& ar)
    {
        // You can save arbitrary data to and from the state extension. This is how it can be done!
        int demo_value = 42;
        std::string demo_string = "hello, Applause!";
        
        // Example: Save a vector of floats (e.g., preset values)
        std::vector<float> demo_vector = {0.1f, 0.5f, 0.75f, 1.0f};
        
        // Example: Save a map (e.g., MIDI CC mappings)
        std::map<int, std::string> demo_map = {
            {1, "foo"},
            {7, "bar"},
            {10, "baz"},
            {74, "qux"}
        };
        
        // Save everything in order. This how to save arbitrary data to the stream.
        ar & demo_value & demo_string & demo_vector & demo_map;

        // Some extensions, like the params, have helper functions to save their state to a given stream.
        params_.saveToStream(ar);
        return true;
    });

    state_.setLoadCallback([this](auto& ar)
    {
        // Load in EXACT SAME ORDER as saved - it's a sequential stream!
        int myValue;
        std::string myString;
        std::vector<float> myPresetValues;
        std::map<int, std::string> midiCCMap;

        ar & myValue & myString & myPresetValues & midiCCMap;
        
        params_.loadFromStream(ar);
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

bool ExampleShowcasePlugin::activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept {
    LOG_INFO("ExampleShowcase::activate() - sampleRate: {}", sampleRate);
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
