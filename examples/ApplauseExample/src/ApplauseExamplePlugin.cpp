#include "ApplauseExamplePlugin.h"
#include <cstring>
#include "util/DebugHelpers.h"
ApplauseExamplePlugin::ApplauseExamplePlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : PluginBase(descriptor, host),
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
    
    // Let the parameter module process events.
    params_.processEvents(process->in_events, process->out_events);
    
    return CLAP_PROCESS_SLEEP;
}
