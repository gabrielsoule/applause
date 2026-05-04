#include "ParamKnob.h"

#include <visage_graphics/font.h>
#include <visage_graphics/theme.h>

#include <algorithm>
#include <cmath>

#include <applause/util/DebugHelpers.h>
#include <embedded/applause_fonts.h>

namespace applause {

VISAGE_THEME_IMPLEMENT_COLOR(ParamKnob, ApplauseParamKnobText, 0xffcccccc);

ParamKnob::ModOverlay::ModOverlay(const ParamKnob& owner) : owner_(&owner) {
    setIgnoresMouseEvents(true, false);
}

void ParamKnob::ModOverlay::draw(visage::Canvas& canvas) {
    if (!owner_->is_connected_) return;

    const float size = std::min(width(), height());
    const float centerX = width() * 0.5f;
    const float centerY = height() * 0.5f;
    const float radius = size * 0.5f;

    constexpr float kTopCenter = 1.5f * static_cast<float>(M_PI);
    const float sweep = canvas.value(Knob::ApplauseKnobSweep);
    const float arcThickness = canvas.value(Knob::ApplauseKnobArcThickness);
    const float trackCenter = radius - canvas.value(Knob::ApplauseKnobTrackInset);
    const float halfSweep = sweep * 0.5f;
    const float startAngle = kTopCenter - halfSweep;
    const float dotDiameter = arcThickness;
    const float dotTrackRadius = trackCenter - arcThickness * 0.5f;

    canvas.setColor(Knob::ApplauseKnobAccent);

    // lambda to handle different behavior for poly vs mono destination
    const auto drawDot = [&](float value) {
        const float norm = std::clamp(owner_->param_info_.toNormalized(value), 0.0f, 1.0f);
        const float angle = startAngle + norm * sweep;
        const float dotCenterX = centerX + std::cos(angle) * dotTrackRadius;
        const float dotCenterY = centerY + std::sin(angle) * dotTrackRadius;
        canvas.circle(dotCenterX - dotDiameter * 0.5f, dotCenterY - dotDiameter * 0.5f, dotDiameter);
    };

    if (owner_->destination_->mode == ModDstMode::Poly) {
        for (uint16_t voice : owner_->mod_matrix_->getActiveVoices()) {
            drawDot(owner_->mod_matrix_->getPolyModValue(owner_->destination_->index, voice));
        }
    } else {
        drawDot(owner_->mod_matrix_->getModValue(owner_->destination_->index));
    }

    redraw();
}

ParamKnob::ParamKnob(ParamInfo& paramInfo) :
    param_info_(paramInfo), mod_overlay_(*this), paramValueText_(paramInfo) {
    setReceiveChildMouseEvents(true);
    knob_.setName(this->name() + " knob");
    addChild(&knob_);
    addChild(&mod_overlay_);
    paramValueText_.setVisible(false);
    addChild(&paramValueText_);

    paramNameText_.setMultiLine(false);
    paramNameText_.setJustification(visage::Font::kCenter);
    paramNameText_.setFont(visage::Font(12, applause::fonts::Jost_Regular_ttf));
    paramNameText_.setActive(false);
    paramNameText_.setText(param_info_.shortName);
    paramNameText_.setIgnoresMouseEvents(true, false);
    paramNameText_.setMargin(0, 0);
    name_text_palette_.setColor(visage::TextEditor::TextEditorText, paletteColor(ApplauseParamKnobText));
    name_text_palette_.setColor(visage::TextEditor::TextEditorBackground, visage::Color(0x00000000));
    paramNameText_.setPalette(&name_text_palette_);
    addChild(&paramNameText_);

    // Push the parameter's default position into the knob (normalized to 0-1).
    // The knob uses this for both the rim-arc origin and double-click reset.
    const float range = param_info_.maxValue - param_info_.minValue;
    knob_.setDefaultValue((param_info_.defaultValue - param_info_.minValue) / range);

    // Set initial value (normalized to 0-1 range)
    const float normalizedValue = (param_info_.getValue() - param_info_.minValue) / range;
    knob_.setValue(normalizedValue);

    // Connect knob value changes to parameter
    knob_.onValueChanged.add([this](float value) {
        const float paramValue =
            this->param_info_.minValue + value * (this->param_info_.maxValue - this->param_info_.minValue);
        this->param_info_.setValueNotifyingHost(paramValue);
    });

    // Connect gesture events
    knob_.onDragStarted.add([this]() { this->param_info_.beginGesture(); });

    knob_.onDragEnded.add([this]() { this->param_info_.endGesture(); });

    // Connect to parameter changes from the host
    param_connection_ = param_info_.on_value_changed.connect([this](float value) {
        // Update knob when parameter changes externally
        const float normalizedValue =
            (value - this->param_info_.minValue) / (this->param_info_.maxValue - this->param_info_.minValue);
        this->knob_.setValue(normalizedValue);
    });
}

void ParamKnob::draw(visage::Canvas& canvas) {
    // No direct text drawing; label rendered via TextEditor child
}

void ParamKnob::resized() {
    knob_.setBounds(0, 0, width(), height() - kLabelHeight);
    mod_overlay_.setBounds(0, 0, width(), height() - kLabelHeight);
    const float label_y = height() - kLabelHeight;
    const float label_h = kLabelHeight - kLabelPadding;
    // The -5, +5 Y axis "out of bounds" margin is due to a quirk of the Visage
    // text editor implementation; it visually clips text BEFORE the left and right borders
    // of the actual component.
    paramValueText_.setBounds(-5, label_y, width() + 10, label_h);
    paramNameText_.setBounds(-5, label_y, width() + 10, label_h);
}

void ParamKnob::setModDestination(ModMatrix* matrix, const ModDestination* dst) {
    ASSERT(matrix);
    ASSERT(dst);
    connections_changed_conn_.disconnect();
    mod_matrix_ = matrix;
    destination_ = dst;
    is_connected_ = matrix->dstIsConnected(dst->index);
    connections_changed_conn_ = matrix->on_connections_changed.connect([this]() { refreshConnectionState(); });
    mod_overlay_.redraw();
}

void ParamKnob::refreshConnectionState() {
    const bool new_state = mod_matrix_->dstIsConnected(destination_->index);
    if (new_state != is_connected_) {
        is_connected_ = new_state;
        mod_overlay_.redraw();
    }
}

void ParamKnob::mouseEnter(const visage::MouseEvent& e) {
    mouseOver_ = true;
    paramValueText_.setVisible(true);
    paramNameText_.setVisible(false);
}

void ParamKnob::mouseExit(const visage::MouseEvent& e) {
    mouseOver_ = false;
    paramValueText_.setVisible(false);
    paramNameText_.setVisible(true);
}
}  // namespace applause
