#pragma once

#include "applause/ui/ApplauseEditor.h"
#include "applause/ui/components/GenericParameterUI.h"
#include "applause/ui/components/ParamKnob.h"
#include "applause/ui/components/ApplauseButton.h"
#include "applause/ui/components/Panel.h"
#include "applause/extensions/ParamsExtension.h"
#include <memory>

class ExampleShowcaseEditor : public applause::ApplauseEditor
{
public:
    explicit ExampleShowcaseEditor(applause::ParamsExtension* params);
    ~ExampleShowcaseEditor() override = default;

    void resized() override;

private:
    // Panels
    applause::Panel knobs_panel_{"Knobs"};
    applause::Panel buttons_panel_{"Buttons"};
    applause::Panel params_panel_{"Parameters"};
    std::unique_ptr<applause::GenericParameterUI> parameter_ui_;

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

    // Small buttons
    std::unique_ptr<applause::UiButton> small_button_;
    std::unique_ptr<applause::ToggleTextButton> small_toggle_button_;

    void onLoadFileClicked();
};
