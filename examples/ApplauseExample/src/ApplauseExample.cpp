#include "ApplauseExample.h"
#include <cstring>
#include "util/DebugHelpers.h"

namespace applause_example {

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

ApplauseExample::ApplauseExample(const clap_host_t* host) 
    : PluginBase(&descriptor, host) {
    LOG_INFO("ApplauseExample constructor");
    
    // Configure extensions
    note_ports_.addInput(applause::NotePortConfig::midi("MIDI In"));
    
    audio_ports_.addOutput(applause::AudioPortConfig::mainStereo("Main Out"));
    
    // Register extensions with the plugin
    registerExtension(note_ports_);
    registerExtension(audio_ports_);
    registerExtension(state_);
}

const clap_plugin_descriptor_t* ApplauseExample::getDescriptor() {
    return &descriptor;
}

bool ApplauseExample::init() noexcept {
    LOG_INFO("ApplauseExample::init()");
    return true;
}

void ApplauseExample::destroy() noexcept {
    LOG_INFO("ApplauseExample::destroy()");
}

bool ApplauseExample::activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept {
    LOG_INFO("ApplauseExample::activate() - sampleRate: {}", sampleRate);
    return true;
}

void ApplauseExample::deactivate() noexcept {
    LOG_INFO("ApplauseExample::deactivate()");
}

clap_process_status ApplauseExample::process(const clap_process_t* process) noexcept {
    
    // Get audio buffers
    if (process->audio_outputs_count < 1) {
        return CLAP_PROCESS_ERROR;
    }
    
    const uint32_t frame_count = process->frames_count;
    const clap_audio_buffer_t* output = &process->audio_outputs[0];
    
    // Process MIDI events
    const clap_event_header_t* event;
    uint32_t event_index = 0;
    
    if (process->in_events) {
        uint32_t event_count = process->in_events->size(process->in_events);
        
        while (event_index < event_count) {
            event = process->in_events->get(process->in_events, event_index);
            
            LOG_INFO("Got an event! Event type is {}", event->type);
            
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
    
    auto* plugin = new ApplauseExample(host);
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

const clap_plugin_factory_t ApplauseExample::factory = {
    .get_plugin_count = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin = factory_create_plugin
};

} // namespace applause_example