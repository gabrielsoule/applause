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
    
    // Create individual knobs for each parameter
    if (getParamsExtension())
    {
        param1_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("param1"));
        addChild(param1_knob_.get());
        
        param2_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("param2"));
        addChild(param2_knob_.get());
        
        filter_mode_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("filter_mode"));
        addChild(filter_mode_knob_.get());
    }
    
    // Create test buttons
    ui_button_ = std::make_unique<applause::UiButton>("UI Button");
    ui_button_->onToggle() += [](applause::Button* button, bool on) {
        LOG_INFO("UI Button clicked!");
    };
    addChild(ui_button_.get());
    
    toggle_button_ = std::make_unique<applause::ToggleTextButton>("Toggle");
    toggle_button_->onToggle() += [](applause::Button* button, bool on) {
        LOG_INFO("Toggle button state: {}", on ? "ON" : "OFF");
    };
    addChild(toggle_button_.get());
}

void ExampleShowcaseEditor::resized()
{
    // Layout components
    parameter_ui_->setBounds(20, 20, 400, 400);
    
    // Position knobs in a vertical column to the right of GenericParameterUI
    const int knob_x = 450;
    const int knob_size = 50;
    const int knob_spacing = 100;
    const int knob_start_y = 40;
    
    if (param1_knob_)
        param1_knob_->setBounds(knob_x, knob_start_y, knob_size, knob_size + 20);
    
    if (param2_knob_)
        param2_knob_->setBounds(knob_x, knob_start_y + knob_spacing, knob_size, knob_size + 20);
    
    if (filter_mode_knob_)
        filter_mode_knob_->setBounds(knob_x, knob_start_y + knob_spacing * 2, knob_size, knob_size + 20);
    
    // Position buttons in a column to the right of the knobs
    const int button_x = knob_x + knob_size + 30;
    const int button_width = 120;
    const int button_height = 40;
    const int button_spacing = 50;
    
    if (ui_button_)
        ui_button_->setBounds(button_x, knob_start_y + 20, button_width, button_height);
    
    if (toggle_button_)
        toggle_button_->setBounds(button_x, knob_start_y + 20 + button_spacing, button_width, button_height);
}
