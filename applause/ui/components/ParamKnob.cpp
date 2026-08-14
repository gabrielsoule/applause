#include "ParamKnob.h"

#include <applause/ui/ApplauseUI.h>

#include <applause/util/DebugHelpers.h>
#include <embedded/applause_fonts.h>

namespace applause {

APPLAUSE_THEME_IMPLEMENT_COLOR(ParamKnob, ApplauseParamKnobText, 0xffcccccc);

ParamKnob::ParamKnob(ParamInfo& paramInfo, const ModDestination* dst) :
    param_info_(paramInfo), paramValueText_(paramInfo) {
    setReceiveChildMouseEvents(true);
    knob_.setName(this->name() + " knob");
    addChild(&knob_);
    paramValueText_.setVisible(false);
    addChild(&paramValueText_);

    paramNameText_.setMultiLine(false);
    paramNameText_.setJustification(applause::Font::kCenter);
    paramNameText_.setFont(applause::Font(12, applause::fonts::Barlow_Medium_ttf));
    paramNameText_.setActive(false);
    paramNameText_.setText(param_info_.shortName);
    paramNameText_.setIgnoresMouseEvents(true, false);
    paramNameText_.setMargin(0, 0);
    paramNameText_.setTextColorId(ApplauseParamKnobText);
    addChild(&paramNameText_);

    // Push the parameter's default position into the knob (normalized to 0-1).
    // The knob uses this for both the rim-arc origin and double-click reset.
    knob_.setDefaultValue(param_info_.toNormalized(param_info_.defaultValue));

    // Set initial value (normalized to 0-1 range)
    knob_.setValue(param_info_.getNormalized());

    // Connect knob value changes to parameter
    knob_.onValueChanged.add([this](float value) {
        this->param_info_.setValueNotifyingHost(this->param_info_.fromNormalized(value));
    });

    // Connect gesture events
    knob_.onDragStarted.add([this]() { this->param_info_.beginGesture(); });

    knob_.onDragEnded.add([this]() { this->param_info_.endGesture(); });

    // Connect to parameter changes from the host
    param_connection_ = param_info_.on_value_changed.connect([this](float value) {
        // Update knob when parameter changes externally
        this->knob_.setValue(this->param_info_.toNormalized(value));
    });

    if (dst) {
        ASSERT(dst->matrix);
        destination_ = dst;
        mod_changed_conn_ = destination_->matrix->on_connections_changed.connect([this] { knob_.redraw(); });
        knob_.setIndicatorProvider([this](std::vector<float>& out, float& arc_min, float& arc_max) {
            const ModMatrixControl* m = destination_->matrix;
            if (!m->dstIsConnected(destination_->index)) return;
            out.resize(m->copyActiveDestinationValues(destination_->index, {}));
            const size_t count = m->copyActiveDestinationValues(destination_->index, out);
            if (count < out.size()) out.resize(count);
            for (float& value : out) value = param_info_.toNormalized(value);
            const auto [off_min, off_max] = m->getModOffsetRange(destination_->index);
            const float v = param_info_.toNormalized(param_info_.getValue());
            if (arc_min >= arc_max) {
                arc_min = v + off_min;
                arc_max = v + off_max;
            }
        });
    }
}

void ParamKnob::setFont(const applause::Font& font) {
    paramNameText_.setFont(font);
    paramNameText_.redraw();
    paramValueText_.setFont(font);
}

void ParamKnob::draw(applause::Canvas& canvas) {
    // No direct text drawing; label rendered via TextEditor child
}

void ParamKnob::resized() {
    knob_.setBounds(0, 0, width(), height() - kLabelHeight);
    const float label_y = height() - kLabelHeight;
    const float label_h = kLabelHeight - kLabelPadding;
    // The -5, +5 Y axis "out of bounds" margin is due to a quirk of the Visage
    // text editor implementation; it visually clips text BEFORE the left and right borders
    // of the actual component.
    paramValueText_.setBounds(-5, label_y, width() + 10, label_h);
    paramNameText_.setBounds(-5, label_y, width() + 10, label_h);
}

void ParamKnob::mouseEnter(const applause::MouseEvent& e) {
    mouseOver_ = true;
    paramValueText_.setVisible(true);
    paramNameText_.setVisible(false);
}

void ParamKnob::mouseExit(const applause::MouseEvent& e) {
    mouseOver_ = false;
    paramValueText_.setVisible(false);
    paramNameText_.setVisible(true);
}
}  // namespace applause
