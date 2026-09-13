#pragma once

#include "SimdPolySynth.h"

#include <applause/core/PluginBase.h>
#include <applause/extensions/AudioPortsExtension.h>
#include <applause/extensions/GUIExtension.h>
#include <applause/extensions/NotePortsExtension.h>
#include <applause/extensions/ParamsExtension.h>
#include <applause/extensions/StateExtension.h>

class ExampleSimdPolySynthPlugin final : public applause::PluginBase {
public:
    explicit ExampleSimdPolySynthPlugin(const clap_plugin_descriptor_t* descriptor, const clap_host_t* host);

    bool activate(const applause::ProcessInfo& info) noexcept override;
    void deactivate() noexcept override;
    void reset() noexcept override;
    applause::ProcessStatus process(applause::ProcessContext& context) noexcept override;

private:
    applause::NotePortsExtension note_ports_;
    applause::AudioPortsExtension audio_ports_;
    applause::ParamsExtension params_{21};
    applause::StateExtension state_;
    SimdPolySynth::ModMatrix mod_matrix_{{SimdPolySynth::batch_count, 8, 32, 64}};
    SimdPolySynth synth_{mod_matrix_};
    applause::GUIExtension gui_ext_;
};
