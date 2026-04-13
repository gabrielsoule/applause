#include "ParamSlider.h"

#include <visage_graphics/canvas.h>

#include <applause/util/DebugHelpers.h>

namespace applause {
ParamSlider::ParamSlider(ParamInfo& paramInfo) : param_info_(paramInfo), param_text_box_(param_info_) {
    addChild(&slider_);
    addChild(&param_text_box_);

    const float currentValue = param_info_.getValue();
    const float normalizedValue = (currentValue - param_info_.minValue) / (param_info_.maxValue - param_info_.minValue);
    slider_.setValue(normalizedValue);

    slider_.on_value_changed.add([this](float value) {
        const float paramValue =
            this->param_info_.minValue + value * (this->param_info_.maxValue - this->param_info_.minValue);
        this->param_info_.setValueNotifyingHost(paramValue);
    });

    slider_.on_drag_started.add([this]() { this->param_info_.beginGesture(); });

    slider_.on_drag_ended.add([this]() { this->param_info_.endGesture(); });

    // Connect to parameter changes from the host
    param_connection_ = param_info_.on_value_changed.connect([this](float value) {
        // Update slider when parameter changes externally
        const float normalizedValue =
            (value - this->param_info_.minValue) / (this->param_info_.maxValue - this->param_info_.minValue);
        this->slider_.setValue(normalizedValue);
    });
}

void ParamSlider::draw(visage::Canvas& canvas) {}

void ParamSlider::resized() {
    // Position the slider on the left side, leaving space for text on the right
    float sliderWidth = width() - kLabelWidth;
    slider_.setBounds(0, 0, sliderWidth, height());

    float textX = width() - kLabelWidth + kLabelPadding;
    float textWidth = kLabelWidth - kLabelPadding;
    param_text_box_.setBounds(textX, 0, textWidth, height());
}
}  // namespace applause
