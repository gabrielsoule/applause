#pragma once

#include <applause/dsp/modulation/MSEGCurve.h>
#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/ApplauseEditor.h>
#include <applause/ui/components/Button.h>
#include <applause/ui/components/Checkbox.h>
#include <applause/ui/components/ParamCheckbox.h>
#include <applause/ui/components/ParamRadioGroup.h>
#include <applause/ui/components/RadioButton.h>
#include <applause/ui/components/RadioGroup.h>
#include <applause/ui/components/SelectionGrid.h>
#include <applause/ui/components/GenericParameterUI.h>
#include <applause/ui/components/Panel.h>
#include <applause/ui/components/ParamSelectionGrid.h>
#include <applause/ui/components/ParamKnob.h>
#include <applause/ui/components/Plot.h>
#include <applause/ui/components/Slider.h>
#include <applause/ui/components/modulation/MSEGDisplay.h>
#include <applause/ui/components/modulation/ModMatrixComponent.h>
#include <memory>
#include <array>

class ExampleShowcaseEditor : public applause::ApplauseEditor {
public:
    ExampleShowcaseEditor(applause::ParamsExtension* params, applause::ModMatrixControl* mod_matrix);
    ~ExampleShowcaseEditor() override = default;

    void resized() override;

private:
    // Panels
    applause::Panel knobs_panel_{"Knobs"};
    applause::Panel buttons_panel_{"Buttons"};
    applause::Panel sliders_panel_{"Sliders"};
    applause::Panel params_panel_{"Parameters"};
    applause::Panel selection_grids_panel_{"Selection & Radios"};
    applause::Panel plots_panel_{"Graphs and Plots"};
    applause::Panel mseg_panel_{"MSEG"};
    applause::Panel mod_matrix_panel_{"Mod Matrix"};
    applause::MSEGCurve<> demo_curve_;
    applause::PlotView mseg_plot_;
    applause::MSEGDisplay mseg_display_;
    applause::SimpleCurve trace_demo_;
    applause::PlotView plot_demo_;
    applause::SimpleCurve plot_trace_demo_;
    std::unique_ptr<applause::GenericParameterUI> parameter_ui_;
    std::unique_ptr<applause::ModMatrixComponent> mod_matrix_ui_;

    // Individual knobs for each parameter
    std::unique_ptr<applause::ParamKnob> param1_knob_;
    std::unique_ptr<applause::ParamKnob> param2_knob_;
    std::unique_ptr<applause::ParamKnob> filter_mode_knob_;

    // Test buttons
    std::unique_ptr<applause::UiButton> ui_button_;
    std::unique_ptr<applause::UiButton> action_button_;
    std::unique_ptr<applause::ToggleTextButton> toggle_button_;
    std::unique_ptr<applause::UiButton> load_file_button_;
#ifdef __APPLE__
    std::unique_ptr<applause::UiButton> popup_menu_button_;
#endif

    // Inactive button
    std::unique_ptr<applause::UiButton> inactive_button_;

#ifndef NDEBUG
    // Debug-build only: toggles the pop-out inspector window owned by the
    // ApplauseEditor base class.
    std::unique_ptr<applause::UiButton> inspector_button_;
#endif

    // Small buttons
    std::unique_ptr<applause::UiButton> small_button_;
    std::unique_ptr<applause::ToggleTextButton> small_toggle_button_;

    applause::Checkbox checkbox_{"Checkbox"};
    applause::Checkbox inactive_checkbox_{"Inactive"};
    applause::Checkbox small_checkbox_{"Small"};
    std::unique_ptr<applause::ParamCheckbox> enabled_checkbox_;
    applause::RadioGroup demo_radio_group_;
    applause::RadioButton first_radio_{"First"};
    applause::RadioButton second_radio_{"Second"};
    applause::RadioButton inactive_radio_{"Inactive"};

    // Sliders
    applause::Slider normal_slider_;
    applause::Slider bipolar_slider_;
    applause::Slider inactive_slider_;

    // Selection grids
    std::unique_ptr<applause::ParamSelectionGrid> filter_mode_grid_;
    applause::SelectionGrid waveform_grid_{2, 3};
    applause::RadioGroup filter_radio_group_;
    std::array<std::unique_ptr<applause::RadioButton>, 6> filter_radios_;
    applause::ParamRadioGroup filter_radio_binding_;

    void onLoadFileClicked();
};
