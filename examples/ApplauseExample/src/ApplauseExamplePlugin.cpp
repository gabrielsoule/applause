#include "ApplauseExamplePlugin.h"
#include <cstring>
#include "util/DebugHelpers.h"

// Plugin descriptor
static const clap_plugin_descriptor_t descriptor = {
    .clap_version = CLAP_VERSION,
    .id = "com.applause.example",
    .name = "Applause Example",
    .vendor = "Applause Framework",
    .url = "https://github.com/example/applause",
    .manual_url = nullptr,
    .support_url = nullptr,
    .version = "0.0.1",
    .description = "Example plugin using the Applause framework",
    .features = (const char*[]){
        CLAP_PLUGIN_FEATURE_INSTRUMENT,
        CLAP_PLUGIN_FEATURE_SYNTHESIZER,
        nullptr
    }
};

ApplauseExamplePlugin::ApplauseExamplePlugin(const clap_host_t* host)
    : PluginBase(&descriptor, host),
      params_(host),
      gui_ext_(host)
{
    LOG_INFO("ApplauseExample constructor");

    // Configure extensions
    note_ports_.addInput(applause::NotePortConfig::midi("MIDI In"));

    audio_ports_.addOutput(applause::AudioPortConfig::mainStereo("Main Out"));

    // Register parameters (ported from HelloClap)
    params_.registerParam(
        applause::ParamBuilder("param1")
        .name("Parameter 1")
        .shortName("Param 1")
        .range(0.0f, 1.0f, 0.5f));

    params_.registerParam(
        applause::ParamBuilder("param2")
        .name("Parameter 2")
        .shortName("Param 2")
        .range(10.0f, 20000.0f, 400.0f));

    params_.registerParam(
        applause::ParamBuilder("filter_mode")
        .name("Filter Mode")
        .shortName("Mode")
        .range(0.0f, 5.0f, 0.0f)
        .isStepped(true));

    // Cache parameter handles for efficient audio thread access
    param1_handle_ = &params_.getHandle("param1");
    param2_handle_ = &params_.getHandle("param2");
    filter_mode_handle_ = &params_.getHandle("filter_mode");

    // Configure state extension callbacks for parameter persistence
    state_.setSaveCallback([this](auto& ar)
    {
        return params_.saveToStream(ar);
    });

    state_.setLoadCallback([this](auto& ar)
    {
        return params_.loadFromStream(ar);
    });

    // Connect params extension to GUI extension
    gui_ext_.registerParamsExtension(&params_);
    
    // Register extensions with the plugin
    registerExtension(note_ports_);
    registerExtension(audio_ports_);
    registerExtension(state_);
    registerExtension(params_);
    registerExtension(gui_ext_);
}

const clap_plugin_descriptor_t* ApplauseExamplePlugin::getDescriptor() {
    return &descriptor;
}

bool ApplauseExamplePlugin::init() noexcept {
    LOG_INFO("ApplauseExample::init()");
    return true;
}

void ApplauseExamplePlugin::destroy() noexcept {
    LOG_INFO("ApplauseExample::destroy()");
}

bool ApplauseExamplePlugin::activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept {
    LOG_INFO("ApplauseExample::activate() - sampleRate: {}", sampleRate);
    return true;
}

void ApplauseExamplePlugin::deactivate() noexcept {
    LOG_INFO("ApplauseExample::deactivate()");
}

clap_process_status ApplauseExamplePlugin::process(const clap_process_t* process) noexcept {
    
    // Process parameter events
    params_.processEvents(process->in_events, process->out_events);
    
    // Get audio buffers
    if (process->audio_outputs_count < 1) {
        return CLAP_PROCESS_ERROR;
    }
    
    const uint32_t frame_count = process->frames_count;
    const clap_audio_buffer_t* output = &process->audio_outputs[0];
    
    // Get current parameter values for use in audio processing
    float param1_value = param1_handle_->getValue();
    float param2_value = param2_handle_->getValue();
    float filter_mode = filter_mode_handle_->getValue();
    
    // Process MIDI events
    const clap_event_header_t* event;
    uint32_t event_index = 0;
    
    if (process->in_events) {
        uint32_t event_count = process->in_events->size(process->in_events);
        
        while (event_index < event_count) {
            event = process->in_events->get(process->in_events, event_index);
            
            // Log non-parameter events (parameter events are handled by params_.processEvents)
            if (event->type != CLAP_EVENT_PARAM_VALUE) {
                LOG_INFO("Got an event! Event type is {}", event->type);
            }
            
            event_index++;
        }
    }

    
    return CLAP_PROCESS_SLEEP;
}

// Plugin factory implementation
static const clap_plugin_t* factory_create_plugin(
    const clap_plugin_factory_t* factory,
    const clap_host_t* host,
    const char* plugin_id) {
    
    if (!plugin_id || std::strcmp(plugin_id, descriptor.id) != 0) {
        return nullptr;
    }
    
    auto* plugin = new ApplauseExamplePlugin(host);
    return plugin->clapPlugin();
}

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* factory) {
    return 1;
}

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(
    const clap_plugin_factory_t* factory,
    uint32_t index) {
    
    if (index == 0) {
        return &descriptor;
    }
    return nullptr;
}

const clap_plugin_factory_t ApplauseExamplePlugin::factory = {
    .get_plugin_count = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin = factory_create_plugin
};
