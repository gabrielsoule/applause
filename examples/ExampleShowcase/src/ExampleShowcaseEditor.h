#pragma once

#include "applause/ui/ApplauseEditor.h"
#include "applause/ui/components/GenericParameterUI.h"
#include "applause/ui/components/ParamKnob.h"
#include "applause/ui/components/ApplauseButton.h"
#include "applause/extensions/ParamsExtension.h"
#include <memory>

class ExampleShowcaseEditor : public applause::ApplauseEditor
{
public:
    explicit ExampleShowcaseEditor(applause::ParamsExtension* params);
    ~ExampleShowcaseEditor() override = default;
    
    void resized() override;
    
private:
    std::unique_ptr<applause::GenericParameterUI> parameter_ui_;
    
    // Individual knobs for each parameter
    std::unique_ptr<applause::ParamKnob> param1_knob_;
    std::unique_ptr<applause::ParamKnob> param2_knob_;
    std::unique_ptr<applause::ParamKnob> filter_mode_knob_;
    
    // Test buttons
    std::unique_ptr<applause::UiButton> ui_button_;
    std::unique_ptr<applause::ToggleTextButton> toggle_button_;
    std::unique_ptr<applause::UiButton> load_file_button_;

    void onLoadFileClicked();
};
