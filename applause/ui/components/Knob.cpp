#include "Knob.h"

#include <algorithm>
#include <cmath>

#include <visage_graphics/theme.h>

#include <applause/util/DebugHelpers.h>

namespace applause {

VISAGE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobBodyTop, 0xFF333338);
VISAGE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobBodyBottom, 0xFF1a1a1e);
VISAGE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobBodyBorder, 0xFF444444);
VISAGE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobArcTrack, 0xFF2a2a2e);
VISAGE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobAccent, 0xff9966ff);

VISAGE_THEME_IMPLEMENT_VALUE(Knob, ApplauseKnobArcThickness, 3.0f);
VISAGE_THEME_IMPLEMENT_VALUE(Knob, ApplauseKnobTrackInset, 1.5f);
VISAGE_THEME_IMPLEMENT_VALUE(Knob, ApplauseKnobSweep, 300.0f * (static_cast<float>(M_PI) / 180.0f));

Knob::Knob() {
    glow_amount_.setSourceValue(0.0f);
    glow_amount_.setTargetValue(1.0f);
    glow_amount_.setAnimationTime(150);
}

void Knob::setValue(float value) {
    value_ = std::clamp(value, 0.0f, 1.0f);
    redraw();
}

void Knob::setDefaultValue(float value) {
    default_value_ = std::clamp(value, 0.0f, 1.0f);
    redraw();
}

void Knob::draw(visage::Canvas& canvas) {
    float size = std::min(width(), height());
    float centerX = width() * 0.5f;
    float centerY = height() * 0.5f;
    float radius = size * 0.5f;

    constexpr float kTopCenter = 1.5f * static_cast<float>(M_PI);  // 270° = 3π/2
    float sweep = canvas.value(ApplauseKnobSweep);
    float arcThickness = canvas.value(ApplauseKnobArcThickness);
    float trackCenter = radius - canvas.value(ApplauseKnobTrackInset);
    float halfSweep = sweep * 0.5f;
    float startAngle = kTopCenter - halfSweep;
    float bodyRadius = radius - 6.0f;
    float bodyDiameter = bodyRadius * 2.0f;

    // Drop shadow beneath knob body
    float shadowOffset = 4.0f;
    float shadowPad = 3.0f;
    float shadowDiameter = bodyDiameter + shadowPad * 2.0f;
    canvas.setColor(0x88000000);
    canvas.fadeCircle(centerX - bodyRadius - shadowPad, centerY - bodyRadius - shadowPad + shadowOffset, shadowDiameter,
                      10.0f);

    // Draw knob body
    float bodyX = centerX - bodyRadius;
    float bodyY = centerY - bodyRadius;
    auto sample = [&](auto id) { return canvas.color(id).gradient().sample(0.0f); };
    canvas.setColor(visage::Brush::vertical(sample(ApplauseKnobBodyTop), sample(ApplauseKnobBodyBottom)));
    canvas.circle(bodyX, bodyY, bodyDiameter);
    canvas.setColor(ApplauseKnobBodyBorder);
    canvas.ring(bodyX, bodyY, bodyDiameter, 1.0f);

    // Animated radial glow over body center
    glow_amount_.update();
    float glowAlpha = glow_amount_.value();
    if (glowAlpha > 0.0f) {
        visage::Color accent = sample(ApplauseKnobAccent);
        visage::Gradient glow;
        glow.addColorStop(accent.withAlpha(glowAlpha * 0.35f), 0.0f);
        glow.addColorStop(accent.withAlpha(glowAlpha * 0.15f), 0.6f);
        glow.addColorStop(accent.withAlpha(glowAlpha * 0.10f), 1.0f);
        visage::Point center = {centerX, centerY};
        canvas.setColor(visage::Brush::radial(glow, center, bodyRadius, bodyRadius));
        canvas.circle(bodyX, bodyY, bodyDiameter);
    }
    if (glow_amount_.isAnimating()) redraw();

    // Draw accent arc on body ring from default position to current value
    if (value_ != default_value_) {
        float accentCenter = startAngle + (value_ + default_value_) * 0.5f * sweep;
        float accentHalfSpan = std::abs(value_ - default_value_) * 0.5f * sweep;
        canvas.setColor(ApplauseKnobAccent);
        canvas.arc(bodyX, bodyY, bodyDiameter, 1.0f, accentCenter, accentHalfSpan, false);
    }

    // Draw arc track
    canvas.setColor(ApplauseKnobArcTrack);
    float trackDiameter = trackCenter * 2.0f;
    canvas.arc(centerX - trackCenter, centerY - trackCenter, trackDiameter, arcThickness, kTopCenter, halfSweep, true);

    // Tracking dot on the arc
    float trackAngleRad = startAngle + value_ * sweep;
    float dotDiameter = arcThickness;
    float dotTrackRadius = trackCenter - arcThickness * 0.5f;
    float dotCenterX = centerX + std::cos(trackAngleRad) * dotTrackRadius;
    float dotCenterY = centerY + std::sin(trackAngleRad) * dotTrackRadius;
    canvas.setColor(ApplauseKnobAccent);
    canvas.circle(dotCenterX - dotDiameter * 0.5f, dotCenterY - dotDiameter * 0.5f, dotDiameter);

    float angle = startAngle + value_ * sweep;

    float needle_radius = bodyDiameter * 0.5f - 2.0f;
    float lineEndX = centerX + static_cast<float>(std::cos(angle)) * needle_radius;
    float lineEndY = centerY + static_cast<float>(std::sin(angle)) * needle_radius;

    // draw it!
    float needleStartRadius = 5.0f;
    float lineStartX = centerX + static_cast<float>(std::cos(angle)) * needleStartRadius;
    float lineStartY = centerY + static_cast<float>(std::sin(angle)) * needleStartRadius;
    canvas.setColor(ApplauseKnobAccent);
    canvas.segment(lineStartX, lineStartY, lineEndX, lineEndY, 3.0f, true);
}

void Knob::mouseDown(const visage::MouseEvent& e) {
    if (e.repeatClickCount() == 2) {
        if (value_ != default_value_) {
            // Wrap with begin/end so the host treats this as a single
            // automatable gesture (matches the mouseWheel pattern below).
            onDragStarted.callback();
            value_ = default_value_;
            onValueChanged.callback(value_);
            onDragEnded.callback();
            redraw();
        }
        return;  // do NOT start a drag on the reset click
    }

    dragging_ = true;
    drag_start_y_ = e.position.y;
    drag_start_value_ = value_;
    glow_amount_.target(true);
    onDragStarted.callback();
    redraw();
}

void Knob::mouseDrag(const visage::MouseEvent& e) {
    if (dragging_) {
        processDrag(e.position.y);
    }
}

void Knob::mouseUp(const visage::MouseEvent& e) {
    if (dragging_) {
        dragging_ = false;
        processDrag(e.position.y);
        onDragEnded.callback();
        if (!hovering_) glow_amount_.target(false);
        redraw();
    }
}

void Knob::mouseEnter(const visage::MouseEvent& e) {
    hovering_ = true;
    glow_amount_.target(true);
    redraw();
}

void Knob::mouseExit(const visage::MouseEvent& e) {
    hovering_ = false;
    if (!dragging_) glow_amount_.target(false);
    redraw();
}

bool Knob::mouseWheel(const visage::MouseEvent& e) {
    float delta = -e.precise_wheel_delta_y * wheel_sensitivity_;
    float newValue = std::clamp(value_ + delta, 0.0f, 1.0f);

    if (newValue != value_) {
        onDragStarted.callback();

        value_ = newValue;
        onValueChanged.callback(value_);
        redraw();

        onDragEnded.callback();

        return true;
    }

    return false;
}

void Knob::processDrag(float mouseY) {
    const float delta_y = drag_start_y_ - mouseY;
    const float delta_value = delta_y * drag_sensitivity_;

    const float newValue = std::clamp(drag_start_value_ + delta_value, 0.0f, 1.0f);

    if (newValue != value_) {
        value_ = newValue;
        onValueChanged.callback(value_);
        redraw();
    }
}

}  // namespace applause
