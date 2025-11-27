#include "ParamKnob.h"

#include <visage_graphics/font.h>

#include "applause/util/DebugHelpers.h"
#include "embedded/applause_fonts.h"

namespace applause {

ParamKnob::ParamKnob(ParamInfo& paramInfo) : param_info_(paramInfo) {
    setReceiveChildMouseEvents(true);
    knob_ = std::make_unique<Knob>();
    knob_->setName(this->name() + " knob");
    addChild(knob_.get());
    paramValueText_ = std::make_unique<ParamValueTextBox>(paramInfo);
    paramValueText_->setVisible(false);
    addChild(paramValueText_.get());

    paramNameText_ = std::make_unique<visage::TextEditor>("param_name");
    paramNameText_->setMultiLine(false);
    paramNameText_->setJustification(visage::Font::kCenter);
    paramNameText_->setFont(visage::Font(12, applause::fonts::JetBrainsMonoNL_Regular_ttf));
    paramNameText_->setActive(false);
    paramNameText_->setText(param_info_.shortName);
    paramNameText_->setIgnoresMouseEvents(true, false);
    paramNameText_->setMargin(0, 0);
    addChild(paramNameText_.get());

    // Set initial value (normalized to 0-1 range)
    const float currentValue = param_info_.getValue();
    const float normalizedValue = (currentValue - param_info_.minValue) /
                                  (param_info_.maxValue - param_info_.minValue);
    knob_->setValue(normalizedValue);

    // Connect knob value changes to parameter
    knob_->onValueChanged.add([this](float value) {
        const float paramValue =
            this->param_info_.minValue +
            value * (this->param_info_.maxValue - this->param_info_.minValue);
        this->param_info_.setValueNotifyingHost(paramValue);
    });

    // Connect gesture events
    knob_->onDragStarted.add([this]() { this->param_info_.beginGesture(); });

    knob_->onDragEnded.add([this]() { this->param_info_.endGesture(); });

    // Connect to parameter changes from the host
    param_connection_ =
        param_info_.on_value_changed.connect([this](float value) {
            // Update knob when parameter changes externally
            const float normalizedValue =
                (value - this->param_info_.minValue) /
                (this->param_info_.maxValue - this->param_info_.minValue);
            this->knob_->setValue(normalizedValue);
        });
}

void ParamKnob::draw(visage::Canvas& canvas) {
    // No direct text drawing; label rendered via TextEditor child
}

void ParamKnob::resized() {
    knob_->setBounds(0, 0, width(), height() - kLabelHeight);
    const float label_y = height() - kLabelHeight;
    const float label_h = kLabelHeight - kLabelPadding;
    // The -5, +5 Y axis "out of bounds" margin is due to a quirk of the Visage
    // text editor implementation; it visually clips text BEFORE the left and right borders
    // of the actual component.
    paramValueText_->setBounds(-5, label_y, width() + 10, label_h);
    if (paramNameText_) {
        paramNameText_->setBounds(-5, label_y, width() + 10, label_h);
    }
}

void ParamKnob::mouseEnter(const visage::MouseEvent& e) {
    mouseOver_ = true;
    paramValueText_->setVisible(true);
    if (paramNameText_) paramNameText_->setVisible(false);
    LOG_DBG("Mouse Enter: {}", e.event_frame->name());
}

void ParamKnob::mouseExit(const visage::MouseEvent& e) {
    mouseOver_ = false;
    paramValueText_->setVisible(false);
    if (paramNameText_) paramNameText_->setVisible(true);
    LOG_DBG("Mouse Exit: {}", e.event_frame->name());
}

}  // namespace applause
