#pragma once

#include "ui/ApplauseEditor.h"
#include "ui/components/GenericParameterUI.h"
#include "ui/components/ParamKnob.h"
#include "extensions/ParamsExtension.h"
#include <memory>

class ExampleShowcaseEditor : public applause::ApplauseEditor
{
public:
    explicit ExampleShowcaseEditor(applause::ParamsExtension* params);
    ~ExampleShowcaseEditor() override = default;
    
    void resized() override;
    
private:
    std::unique_ptr<applause::GenericParameterUI> parameter_ui_;
    std::unique_ptr<applause::ParamKnob> test_param_knob_;
};
