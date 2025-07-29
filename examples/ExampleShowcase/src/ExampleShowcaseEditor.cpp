#include "ExampleShowcaseEditor.h"
#include "util/DebugHelpers.h"
#include <visage_graphics/canvas.h>

using namespace visage::dimension;

ExampleShowcaseEditor::ExampleShowcaseEditor(applause::ParamsExtension* params)
    : applause::ApplauseEditor(params)
{
    ApplauseEditor::setFixedAspectRatio(true);

    // Create parameter UI
    parameter_ui_ = std::make_unique<applause::GenericParameterUI>();
    
    // Add all non-internal parameters to the UI
    if (getParamsExtension())
    {
        for (auto& param : getParamsExtension()->getAllParameters())
        {
            if (!param.internal)
            {
                parameter_ui_->addParameter(param);
            }
        }
    }
    
    addChild(parameter_ui_.get());
    
    // Create a test knob for param2
    if (getParamsExtension())
    {
        test_param_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("param2"));
        addChild(test_param_knob_.get());
    }
}

void ExampleShowcaseEditor::draw(visage::Canvas& canvas)
{
    // Black background
    canvas.setColor(0xFF000000);
    canvas.fill(0, 0, width(), height());
}

void ExampleShowcaseEditor::resized()
{
    // Layout components
    parameter_ui_->setBounds(20, 20, 400, 400);
    
    if (test_param_knob_)
    {
        test_param_knob_->setBounds(440, 80, 50, 50 + 20);
    }
}
