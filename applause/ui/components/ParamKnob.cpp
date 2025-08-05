#include "ParamKnob.h"
#include <visage_graphics/font.h>
#include "embedded/applause_fonts.h"
#include "applause/util/DebugHelpers.h"

namespace applause {

ParamKnob::ParamKnob(ParamInfo& paramInfo) : param_info_(paramInfo)
{
    knob_ = std::make_unique<Knob>();
    addChild(knob_.get());
    
    // Set initial value (normalized to 0-1 range)
    const float currentValue = param_info_.getValue();
    const float normalizedValue = (currentValue - param_info_.minValue) /
                                 (param_info_.maxValue - param_info_.minValue);
    knob_->setValue(normalizedValue);
    
    // Connect knob value changes to parameter
    knob_->onValueChanged.add([this](float value)
    {
        const float paramValue = this->param_info_.minValue +
                               value * (this->param_info_.maxValue - this->param_info_.minValue);
        this->param_info_.setValueNotifyingHost(paramValue);
    });
    
    // Connect gesture events
    knob_->onDragStarted.add([this]()
    {
        this->param_info_.beginGesture();
    });
    
    knob_->onDragEnded.add([this]()
    {
        this->param_info_.endGesture();
    });
    
    // Connect to parameter changes from the host
    param_connection_ = param_info_.on_value_changed.connect([this](float value)
    {
        // Update knob when parameter changes externally
        const float normalizedValue = (value - this->param_info_.minValue) /
                                     (this->param_info_.maxValue - this->param_info_.minValue);
        this->knob_->setValue(normalizedValue);
    });
}

void ParamKnob::draw(visage::Canvas& canvas)
{
    // Draw the parameter short name at the bottom
    const visage::Font font(12, applause::fonts::JetBrainsMonoNL_Regular_ttf);
    
    // Calculate text area bounds (bottom of the component)
    float textY = height() - kLabelHeight;
    float textWidth = width();
    float textHeight = kLabelHeight - kLabelPadding;
    
    // Draw the short name centered at the bottom
    canvas.setColor(0xFFFFFFFF);
    canvas.text(param_info_.shortName, font, visage::Font::kCenter,
                0, textY + kLabelPadding, textWidth, textHeight);
}

void ParamKnob::resized()
{
    // Position the knob in the remaining space above the label
    float knobHeight = height() - kLabelHeight;
    if (knob_)
    {
        knob_->setBounds(0, 0, width(), knobHeight);
    }
}

} // namespace applause