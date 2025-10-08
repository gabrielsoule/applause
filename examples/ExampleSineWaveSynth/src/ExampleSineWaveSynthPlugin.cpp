#include "ExampleSineWaveSynthPlugin.h"

#include "applause/util/DebugHelpers.h"

ExampleSineWaveSynthPlugin::ExampleSineWaveSynthPlugin(
    const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : PluginBase(descriptor, host) {
    LOG_INFO("ExampleSineWaveSynth constructor");

    note_ports_.addInput(applause::NotePortConfig::universal("MIDI In"));
    audio_ports_.addOutput(applause::AudioPortConfig::mainStereo("Main Out"));

    registerExtension(note_ports_);
    registerExtension(audio_ports_);
    registerExtension(state_);
}

bool ExampleSineWaveSynthPlugin::init() noexcept {
    LOG_INFO("ExampleSineWaveSynth::init()");
    return true;
}

void ExampleSineWaveSynthPlugin::destroy() noexcept {
    LOG_INFO("ExampleSineWaveSynth::destroy()");
}

bool ExampleSineWaveSynthPlugin::activate(const applause::ProcessInfo& info) noexcept {
    LOG_INFO("ExampleSineWaveSynth::activate() - sampleRate: {}", info.sample_rate);
    sample_rate_ = info.sample_rate;
    synth_.activate(info);
    return true;
}

void ExampleSineWaveSynthPlugin::deactivate() noexcept {
    LOG_INFO("ExampleSineWaveSynth::deactivate()");
}

clap_process_status ExampleSineWaveSynthPlugin::process(
    const clap_process_t* process) noexcept {
    if (process->audio_outputs_count == 0) {
        return CLAP_PROCESS_SLEEP;
    }

    const clap_audio_buffer_t* output = &process->audio_outputs[0];

    applause::BufferView<float, 2> buffer(output->data32, output->channel_count, process->frames_count);

    synth_.process(buffer, process->in_events);

    return CLAP_PROCESS_CONTINUE;
}
