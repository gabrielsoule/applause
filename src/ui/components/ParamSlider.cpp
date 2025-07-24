#include "ParamSlider.h"
#include <visage_graphics/font.h>
#include <sstream>
#include <iomanip>
#include <extensions/ParamsExtension.h>

#include "embedded/fonts.h"
#include "util/DebugHelpers.h"

namespace applause {

ParamSlider::ParamSlider(ParamInfo& paramInfo) : param_info_(paramInfo)
{
    slider_ = std::make_unique<Slider>();
    addChild(slider_.get());

    // Set initial value (normalized to 0-1 range)
    const float currentValue = param_info_.getValue();
    const float normalizedValue = (currentValue - param_info_.minValue) / (param_info_.maxValue - param_info_.minValue);
    slider_->setValue(normalizedValue);
    LOG_DBG("ParamSlider value: {} min: {} max: {}", param_info_.getValue(), param_info_.minValue, param_info_.maxValue);

    // Add our callbacks to hook the slider's state into the parameter's state
    slider_->on_value_changed.add([this](float value)
    {
        const float paramValue = this->param_info_.minValue + value * (this->param_info_.maxValue - this->param_info_.
            minValue);
        this->param_info_.setValueNotifyingHost(paramValue);
        redraw();
    });

    // Connect gesture events
    slider_->on_drag_started.add([this]()
    {
        this->param_info_.beginGesture();
    });

    slider_->on_drag_ended.add([this]()
    {
        this->param_info_.endGesture();
    });

    // Connect to parameter changes from the host
    param_connection_ = param_info_.on_value_changed.connect([this](float value)
    {
        // Update slider when parameter changes externally
        const float normalizedValue = (value - this->param_info_.minValue) /
            (this->param_info_.maxValue - this->param_info_.minValue);
        this->slider_->setValue(normalizedValue);
        this->redraw(); // Update the text display
        LOG_DBG("Rocket message received! Param value changed to {}", value);
    });
}

void ParamSlider::draw(visage::Canvas& canvas)
{
    float currentValue = param_info_.getValue();

    // Format the value as a string
    std::ostringstream valueStream;
    if (param_info_.stepped)
    {
        valueStream << static_cast<int>(currentValue);
    }
    else
    {
        valueStream << std::fixed << std::setprecision(2) << currentValue;
    }

    // Add unit if available
    std::string valueText = valueStream.str();

    // Calculate text area bounds (right side of the component)
    float textX = width() - kLabelWidth;
    float textY = 0;
    float textWidth = kLabelWidth - kLabelPadding;
    float textHeight = height();

    // Draw the text
    const visage::Font font(14, visage::fonts::DroidSansMono_ttf);
    canvas.setColor(0xFFFFFFFF);
    canvas.text(valueText, font, visage::Font::kCenter, textX + kLabelPadding, textY, textWidth, textHeight);
}


void ParamSlider::resized()
{
    // Position the slider on the left side, leaving space for text on the right
    float sliderWidth = width() - kLabelWidth;
    if (slider_)
    {
        slider_->setBounds(0, 0, sliderWidth, height());
    }
}

} // namespace applause
