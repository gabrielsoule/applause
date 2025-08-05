#include "ExampleFilterEditor.h"
#include "applause/util/DebugHelpers.h"
#include <visage_graphics/canvas.h>

using namespace visage::dimension;

ExampleFilterEditor::ExampleFilterEditor(applause::ParamsExtension* params)
    : applause::ApplauseEditor(params)
{
    ApplauseEditor::setFixedAspectRatio(true);
    
    // Create knobs for the three filter parameters
    cutoff_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("filter_cutoff"));
    resonance_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("filter_resonance"));
    mode_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("filter_mode"));
    
    addChild(cutoff_knob_.get());
    addChild(resonance_knob_.get());
    addChild(mode_knob_.get());
}

void ExampleFilterEditor::resized()
{
    // Calculate knob dimensions
    const float knob_width = 60.0f;
    const float knob_height = 60.0f + 20.0f; // Knob + label height
    const float spacing = 30.0f;
    const float total_width = (knob_width * 3) + (spacing * 2);
    
    // Center the knobs horizontally
    const float start_x = (width() - total_width) / 2.0f;
    const float y = (height() - knob_height) / 2.0f;
    
    // Position each knob in a row
    cutoff_knob_->setBounds(start_x, y, knob_width, knob_height);
    resonance_knob_->setBounds(start_x + knob_width + spacing, y, knob_width, knob_height);
    mode_knob_->setBounds(start_x + (knob_width + spacing) * 2, y, knob_width, knob_height);
}