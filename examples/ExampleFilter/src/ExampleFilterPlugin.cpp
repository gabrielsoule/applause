#include "ExampleFilterPlugin.h"
#include "ExampleFilterEditor.h"
#include <cstring>
#include "applause/util/DebugHelpers.h"
#include <clap/events.h>
#include <nlohmann/json.hpp>

ExampleFilterPlugin::ExampleFilterPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host)
    : PluginBase(descriptor, host),
      params_(host),
      gui_ext_(host,
               [this]() { return std::make_unique<ExampleFilterEditor>(&params_); },
               340, 180)
{
    LOG_INFO("ExampleFilter constructor");

    // Configure audio ports for stereo effect processing
    audio_ports_.addInput(applause::AudioPortConfig::mainStereo("Audio In"));

    audio_ports_.addOutput(applause::AudioPortConfig::mainStereo("Audio Out"));

    // Register filter cutoff parameter
    params_.registerParam(applause::ParamConfig{
        .string_id = "filter_cutoff",
        .name = "Filter Cutoff",
        .short_name = "Cutoff",
        .unit = "Hz",
        .min_value = 20.0f, // 20 Hz minimum
        .max_value = 20000.0f, // 20 kHz maximum
        .default_value = 500.0f // 500 Hz default
    });

    // Register filter resonance parameter
    params_.registerParam(applause::ParamConfig{
        .string_id = "filter_resonance",
        .name = "Filter Resonance",
        .short_name = "Resonance",
        .min_value = 0.1f, // Minimum Q
        .max_value = 10.0f, // Maximum Q
        .default_value = 0.71f // Default Q
    });

    params_.registerParam(applause::ParamConfig{
        .string_id = "filter_mode",
        .name = "Filter Mode",
        .short_name = "Mode",
        .min_value = 0.0f,
        .max_value = 1.0f,
        .default_value = 0.0f,
    });


    param_cutoff_ = &params_.getHandle("filter_cutoff");
    param_res_ = &params_.getHandle("filter_resonance");
    param_mode_ = &params_.getHandle("filter_mode");


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

    // Register extensions
    registerExtension(audio_ports_);
    registerExtension(params_);
    registerExtension(state_);
    registerExtension(gui_ext_);
}

bool ExampleFilterPlugin::init() noexcept
{
    LOG_INFO("ExampleFilter init");
    return true;
}

void ExampleFilterPlugin::destroy() noexcept
{
    LOG_INFO("ExampleFilter destroy");
}

bool ExampleFilterPlugin::activate(double sample_rate, uint32_t min_frames, uint32_t max_frames) noexcept
{
    sample_rate_ = sample_rate;

    juce::dsp::ProcessSpec spec{};
    spec.sampleRate = sample_rate;
    spec.maximumBlockSize = max_frames;
    spec.numChannels = 2;

    filter_.prepare(spec);
    filter_.reset();

    const float initial_cutoff = param_cutoff_->getValue();
    const float initial_resonance = param_res_->getValue();
    const float initial_mode = param_mode_->getValue();

    filter_.setCutoffFrequency(initial_cutoff);
    filter_.setQValue(initial_resonance);
    filter_.setMode(initial_mode); // 0 = lowpass, 0.5 = bandpass, 1.0 = highpass

    // Update our cached values
    last_cutoff_ = initial_cutoff;
    last_resonance_ = initial_resonance;
    last_mode_ = initial_mode;

    return true;
}

void ExampleFilterPlugin::deactivate() noexcept
{
    LOG_INFO("ExampleFilter deactivate");
}

clap_process_status ExampleFilterPlugin::process(const clap_process_t* process) noexcept
{
    // Ensure we have matching input and output configurations
    if (process->audio_inputs_count == 0 || process->audio_outputs_count == 0)
    {
        return CLAP_PROCESS_CONTINUE;
    }

    // We only define 1 input audio port and 1 output audio port.
    // As such, we grab the 0th input and output, knowing that len(audio_inputs) = len(audio_outputs) = 1.
    const clap_audio_buffer_t* input = &process->audio_inputs[0];
    clap_audio_buffer_t* output = &process->audio_outputs[0];

    const uint32_t channel_count = std::min(input->channel_count, output->channel_count); // should usually be 2!
    const uint32_t frame_count = process->frames_count;

    // Process parameter events through ParamsExtension
    if (process->in_events && process->out_events)
    {
        params_.processEvents(process->in_events, process->out_events);
    }

    // Check for parameter changes using cached handles (efficient audio-thread access)
    const float current_cutoff = param_cutoff_->getValue();
    const float current_resonance = param_res_->getValue();
    const float current_mode = param_mode_->getValue();

    // Only update filter if parameters actually changed (our own dirty checking)
    if (current_cutoff != last_cutoff_)
    {
        filter_.setCutoffFrequency(current_cutoff);
        last_cutoff_ = current_cutoff;
    }

    if (current_resonance != last_resonance_)
    {
        filter_.setQValue(current_resonance);
        last_resonance_ = current_resonance;
    }

    if (current_mode != last_mode_)
    {
        filter_.setMode(current_mode);
        last_mode_ = current_mode;
    }

    // Process audio if we have valid buffers
    if (input->data32 && output->data32 && channel_count > 0 && frame_count > 0)
    {
        // Copy input to output first (for in-place processing)
        for (uint32_t ch = 0; ch < channel_count; ++ch)
        {
            std::memcpy(output->data32[ch], input->data32[ch], frame_count * sizeof(float));
        }

        // Create BufferView directly from CLAP output data for in-place processing
        chowdsp::BufferView<float> buffer_view(output->data32, static_cast<int>(channel_count),
                                               static_cast<int>(frame_count));

        filter_.processBlock(buffer_view);
    }

    return CLAP_PROCESS_CONTINUE;
}
