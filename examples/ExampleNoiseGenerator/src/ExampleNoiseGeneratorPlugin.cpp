#include "ExampleNoiseGeneratorPlugin.h"
#include <cstring>
#include "util/DebugHelpers.h"

ExampleNoiseGeneratorPlugin::ExampleNoiseGeneratorPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : PluginBase(descriptor, host),
      noise_seed_(1)  // Initialize with non-zero seed
{
    LOG_INFO("ExampleNoiseGenerator constructor");

    // Configure audio ports - stereo output only, no input
    audio_ports_.addOutput(applause::AudioPortConfig::mainStereo("Main Out"));
    
    // Register extensions with the plugin
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

bool ExampleNoiseGeneratorPlugin::activate(double sampleRate, uint32_t minFrameCount, uint32_t maxFrameCount) noexcept {
    LOG_INFO("ExampleNoiseGenerator::activate() - sampleRate: {}", sampleRate);
    return true;
}

void ExampleNoiseGeneratorPlugin::deactivate() noexcept {
    LOG_INFO("ExampleNoiseGenerator::deactivate()");
}

clap_process_status ExampleNoiseGeneratorPlugin::process(const clap_process_t* process) noexcept {
    // Check if we have outputs
    if (process->audio_outputs_count == 0) {
        return CLAP_PROCESS_SLEEP;
    }
    
    // Get the output buffer
    const clap_audio_buffer_t* output = &process->audio_outputs[0];
    
    // Generate white noise for each frame
    for (uint32_t i = 0; i < process->frames_count; ++i) {
        // Fast PRNG: Linear congruential generator
        noise_seed_ = noise_seed_ * 196314165 + 907633515;
        int temp = static_cast<int>(noise_seed_ >> 7) - 16777216;
        float sample = static_cast<float>(temp) / 16777216.0f;
        
        // Write the same sample to both channels (stereo)
        if (output->channel_count >= 1 && output->data32[0]) {
            output->data32[0][i] = sample;
        }
        if (output->channel_count >= 2 && output->data32[1]) {
            output->data32[1][i] = sample;
        }
    }
    
    return CLAP_PROCESS_CONTINUE;
}