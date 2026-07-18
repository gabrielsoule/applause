#include "ExampleNoiseGeneratorPlugin.h"
#include <applause/util/DebugHelpers.h>

ExampleNoiseGeneratorPlugin::ExampleNoiseGeneratorPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : PluginBase(descriptor, host),
      noise_seed_(1)  // Initialize with non-zero seed
{
    LOG_INFO("ExampleNoiseGenerator constructor");

    // Provide a MIDI/Event input bus to satisfy VST3 Instrument expectations in strict hosts
    note_ports_.addInput(applause::NotePortConfig::midi("MIDI In"));

    // Configure audio ports - stereo output only, no input
    audio_ports_.addOutput(applause::AudioPortConfig::mainStereo("Main Out"));
    
    // Register extensions with the plugin
    registerExtension(note_ports_);
    registerExtension(audio_ports_);

    // The state extension doesn't do anything since we have no parameters,
    // However, some DAWs (e.g. Bitwig) complain when a plugin doesn't implement state properly.
    // This is annoying to the user, so we mitigate here by registering a state extension, even though we don't have state.
    registerExtension(state_);
}

bool ExampleNoiseGeneratorPlugin::init() noexcept {
    LOG_INFO("ExampleNoiseGenerator::init()");
    return true;
}

void ExampleNoiseGeneratorPlugin::destroy() noexcept {
    LOG_INFO("ExampleNoiseGenerator::destroy()");
}

bool ExampleNoiseGeneratorPlugin::activate(const applause::ProcessInfo& info) noexcept {
    LOG_INFO("ExampleNoiseGenerator::activate() - sampleRate: {}", info.sample_rate);
    return true;
}

void ExampleNoiseGeneratorPlugin::deactivate() noexcept {
    LOG_INFO("ExampleNoiseGenerator::deactivate()");
}

applause::ProcessStatus ExampleNoiseGeneratorPlugin::process(applause::ProcessContext& context) noexcept {
    // Check if we have outputs
    if (context.audioOutputs().empty()) {
        return applause::ProcessStatus::Sleep;
    }

    auto output = context.output<float, 2>();
    output.clear();

    // Generate white noise for each frame
    for (std::size_t frame = 0; frame < context.numFrames(); ++frame) {
        // Fast PRNG: Linear congruential generator
        noise_seed_ = noise_seed_ * 196314165 + 907633515;
        int temp = static_cast<int>(noise_seed_ >> 7) - 16777216;
        float sample = static_cast<float>(temp) / 16777216.0f;

        for (std::size_t channel = 0; channel < output.numChannels(); ++channel)
            output.channel(channel).store(frame, sample);
    }

    return applause::ProcessStatus::Continue;
}
