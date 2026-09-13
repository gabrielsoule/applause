#pragma once

#include <applause/core/ModMatrix.h>
#include <applause/dsp/modulation/MSEGCurve.h>
#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/ApplauseEditor.h>
#include <applause/ui/components/Panel.h>
#include <applause/ui/components/ParamSelectionGrid.h>
#include <applause/ui/components/ParamKnob.h>
#include <applause/ui/components/Plot.h>
#include <applause/ui/components/modulation/MSEGDisplay.h>
#include <applause/ui/components/modulation/ModMatrixComponent.h>

#include <array>
#include <memory>

class ExampleSimdPolySynthEditor final : public applause::ApplauseEditor {
public:
    ExampleSimdPolySynthEditor(applause::ParamsExtension* params,
                               applause::ModMatrixControl* mod_matrix);
    ~ExampleSimdPolySynthEditor() override = default;

    void resized() override;

private:
    applause::Panel osc1_panel_{"Oscillator 1"};
    applause::Panel osc2_panel_{"Oscillator 2"};
    applause::Panel filter_panel_{"Filter"};
    applause::Panel envelope_panel_{"Envelope"};
    applause::Panel mod_matrix_panel_{"Mod Matrix"};

    std::unique_ptr<applause::ParamSelectionGrid> osc1_waveform_grid_;
    std::unique_ptr<applause::ParamSelectionGrid> osc2_waveform_grid_;
    std::array<std::unique_ptr<applause::ParamKnob>, 5> osc1_knobs_;
    std::array<std::unique_ptr<applause::ParamKnob>, 5> osc2_knobs_;

    applause::MSEGCurve<> filter_curve_;
    applause::PlotView filter_plot_;
    applause::MSEGDisplay filter_display_;
    std::unique_ptr<applause::ParamSelectionGrid> filter_type_grid_;
    std::array<std::unique_ptr<applause::ParamKnob>, 4> filter_knobs_;

    applause::MSEGCurve<> envelope_curve_;
    applause::PlotView envelope_plot_;
    applause::MSEGDisplay envelope_display_;
    std::array<std::unique_ptr<applause::ParamKnob>, 4> envelope_knobs_;

    std::unique_ptr<applause::ModMatrixComponent> mod_matrix_ui_;
};
