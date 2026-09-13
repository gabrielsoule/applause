#include "ExampleSimdPolySynthPlugin.h"
#include "ExampleSimdPolySynthEditor.h"

#include <string>

ExampleSimdPolySynthPlugin::ExampleSimdPolySynthPlugin(const clap_plugin_descriptor_t* descriptor,
                                                       const clap_host_t* host) :
    PluginBase(descriptor, host),
    gui_ext_([this] {
        return std::make_unique<ExampleSimdPolySynthEditor>(&params_, &mod_matrix_);
    }, 1280, 800, true) {
    note_ports_.addInput(applause::NotePortConfig::clap("Note In"));
    audio_ports_.addOutput(applause::AudioPortConfig::mainStereo("Main Out"));

    const auto registerOscillator = [this](const std::string& id_prefix, const std::string& module) {
        params_.registerParam({.string_id = id_prefix + "_waveform",
                               .name = "Waveform",
                               .module = module,
                               .short_name = "Waveform",
                               .default_value = 0.0f,
                               .choices = {"SIN", "TRI", "SAW", "SQR"}});
        params_.registerParam({.string_id = id_prefix + "_octave",
                               .name = "Octave",
                               .module = module,
                               .short_name = "Octave",
                               .unit = "oct",
                               .min_value = -2.0f,
                               .max_value = 2.0f,
                               .default_value = 0.0f,
                               .is_stepped = true,
                               .is_polyphonic = true});
        params_.registerParam({.string_id = id_prefix + "_semitone",
                               .name = "Semitone",
                               .module = module,
                               .short_name = "Semitone",
                               .unit = "st",
                               .min_value = -12.0f,
                               .max_value = 12.0f,
                               .default_value = 0.0f,
                               .is_stepped = true,
                               .is_polyphonic = true});
        params_.registerParam({.string_id = id_prefix + "_fine",
                               .name = "Fine Tune",
                               .module = module,
                               .short_name = "Fine",
                               .unit = "ct",
                               .min_value = -100.0f,
                               .max_value = 100.0f,
                               .default_value = 0.0f,
                               .is_polyphonic = true});
        params_.registerParam({.string_id = id_prefix + "_level",
                               .name = "Level",
                               .module = module,
                               .short_name = "Level",
                               .min_value = 0.0f,
                               .max_value = 1.0f,
                               .default_value = 1.0f,
                               .is_polyphonic = true});
        params_.registerParam({.string_id = id_prefix + "_pan",
                               .name = "Pan",
                               .module = module,
                               .short_name = "Pan",
                               .min_value = -1.0f,
                               .max_value = 1.0f,
                               .default_value = 0.0f,
                               .is_polyphonic = true});
    };

    registerOscillator("osc1", "Oscillator 1");
    registerOscillator("osc2", "Oscillator 2");

    params_.registerParam({.string_id = "filter_type",
                           .name = "Filter Type",
                           .module = "Filter",
                           .short_name = "Type",
                           .default_value = 0.0f,
                           .choices = {"Low-pass", "Band-pass", "High-pass"}});
    params_.registerParam({.string_id = "filter_cutoff",
                           .name = "Cutoff",
                           .module = "Filter",
                           .short_name = "Cutoff",
                           .unit = "Hz",
                           .min_value = 20.0f,
                           .max_value = 20000.0f,
                           .default_value = 1000.0f,
                           .is_polyphonic = true,
                           .scaling = applause::ValueScaling::frequency(20.0f, 20000.0f)});
    params_.registerParam({.string_id = "filter_resonance",
                           .name = "Resonance",
                           .module = "Filter",
                           .short_name = "Resonance",
                           .min_value = 0.1f,
                           .max_value = 10.0f,
                           .default_value = 0.71f,
                           .is_polyphonic = true});
    params_.registerParam({.string_id = "filter_drive",
                           .name = "Drive",
                           .module = "Filter",
                           .short_name = "Drive",
                           .min_value = 0.0f,
                           .max_value = 1.0f,
                           .default_value = 0.0f,
                           .is_polyphonic = true});
    params_.registerParam({.string_id = "filter_env_amount",
                           .name = "Envelope Amount",
                           .module = "Filter",
                           .short_name = "Env Amt",
                           .min_value = -1.0f,
                           .max_value = 1.0f,
                           .default_value = 0.0f,
                           .is_polyphonic = true});

    params_.registerParam({.string_id = "attack",
                           .name = "Attack",
                           .module = "Envelope",
                           .short_name = "Attack",
                           .unit = "s",
                           .min_value = 0.001f,
                           .max_value = 2.0f,
                           .default_value = 0.01f,
                           .is_polyphonic = true,
                           .scaling = applause::ValueScaling::time(0.001f, 2.0f)});
    params_.registerParam({.string_id = "decay",
                           .name = "Decay",
                           .module = "Envelope",
                           .short_name = "Decay",
                           .unit = "s",
                           .min_value = 0.001f,
                           .max_value = 2.0f,
                           .default_value = 0.1f,
                           .is_polyphonic = true,
                           .scaling = applause::ValueScaling::time(0.001f, 2.0f)});
    params_.registerParam({.string_id = "sustain",
                           .name = "Sustain",
                           .module = "Envelope",
                           .short_name = "Sustain",
                           .min_value = 0.0f,
                           .max_value = 1.0f,
                           .default_value = 0.7f,
                           .is_polyphonic = true});
    params_.registerParam({.string_id = "release",
                           .name = "Release",
                           .module = "Envelope",
                           .short_name = "Release",
                           .unit = "s",
                           .min_value = 0.001f,
                           .max_value = 5.0f,
                           .default_value = 0.3f,
                           .is_polyphonic = true,
                           .scaling = applause::ValueScaling::time(0.001f, 5.0f)});

    mod_matrix_.registerFromParamsExtension(params_);
    synth_.bindParameters();

    state_.setSaveCallback([this](applause::json& json) {
        return params_.saveToJson(json["parameters"]);
    });
    state_.setLoadCallback([this](const applause::json& json) {
        if (!json.contains("parameters")) return true;
        return params_.loadFromJson(json["parameters"]);
    });

    registerExtension(note_ports_);
    registerExtension(audio_ports_);
    registerExtension(state_);
    registerExtension(params_);
    registerExtension(gui_ext_);
}

bool ExampleSimdPolySynthPlugin::activate(const applause::ProcessInfo& info) noexcept {
    synth_.activate(info);
    return true;
}

void ExampleSimdPolySynthPlugin::deactivate() noexcept { synth_.reset(); }

void ExampleSimdPolySynthPlugin::reset() noexcept { synth_.reset(); }

applause::ProcessStatus ExampleSimdPolySynthPlugin::process(applause::ProcessContext& context) noexcept {
    params_.processEvents(context.inputEvents(), context.outputEvents());
    mod_matrix_.loadParamBaseValues(params_);
    if (context.audioOutputs().empty()) {
        return applause::ProcessStatus::Sleep;
    }

    synth_.process(context.output<float>(), context.inputEvents());
    return synth_.empty() ? applause::ProcessStatus::Sleep : applause::ProcessStatus::Continue;
}
