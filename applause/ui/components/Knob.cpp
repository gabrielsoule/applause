#include "Knob.h"

#include <applause/ui/ApplauseUI.h>

#include <algorithm>
#include <cmath>

#include <applause/util/DebugHelpers.h>

namespace applause {

APPLAUSE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobBodyTop, 0xFF333338);
APPLAUSE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobBodyBottom, 0xFF1a1a1e);
APPLAUSE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobBodyBorder, 0xFF444444);
APPLAUSE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobArcTrack, 0xFF2a2a2e);
APPLAUSE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobAccent, 0xff9966ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(Knob, ApplauseKnobShadow, 0x88000000);

APPLAUSE_THEME_IMPLEMENT_VALUE(Knob, ApplauseKnobArcThickness, 3.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Knob, ApplauseKnobTrackInset, 1.5f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Knob, ApplauseKnobSweep, 300.0f * (static_cast<float>(M_PI) / 180.0f));

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

void Knob::setIndicatorProvider(
    std::function<void(std::vector<float>&, float&, float&)> provider) {
    indicator_provider_ = std::move(provider);
    redraw();
}

void Knob::draw(applause::Canvas& canvas) {
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
    canvas.setColor(ApplauseKnobShadow);
    canvas.fadeCircle(centerX - bodyRadius - shadowPad, centerY - bodyRadius - shadowPad + shadowOffset, shadowDiameter,
                      10.0f);

    // Draw knob body
    float bodyX = centerX - bodyRadius;
    float bodyY = centerY - bodyRadius;
    auto sample = [&](auto id) { return canvas.color(id).gradient().sample(0.0f); };
    canvas.setColor(applause::Brush::vertical(sample(ApplauseKnobBodyTop), sample(ApplauseKnobBodyBottom)));
    canvas.circle(bodyX, bodyY, bodyDiameter);
    canvas.setColor(ApplauseKnobBodyBorder);
    canvas.ring(bodyX, bodyY, bodyDiameter, 1.0f);

    // Animated radial glow over body center
    glow_amount_.update();
    float glowAlpha = glow_amount_.value();
    if (glowAlpha > 0.0f) {
        applause::Color accent = sample(ApplauseKnobAccent);
        applause::Gradient glow;
        glow.addColorStop(accent.withAlpha(glowAlpha * 0.35f), 0.0f);
        glow.addColorStop(accent.withAlpha(glowAlpha * 0.15f), 0.6f);
        glow.addColorStop(accent.withAlpha(glowAlpha * 0.10f), 1.0f);
        applause::Point center = {centerX, centerY};
        canvas.setColor(applause::Brush::radial(glow, center, bodyRadius, bodyRadius));
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

    indicator_buf_.clear();
    float arc_min = 1.0f, arc_max = 0.0f;  // sentinel: arc_min >= arc_max → no range arc
    if (indicator_provider_) indicator_provider_(indicator_buf_, arc_min, arc_max);

    if (arc_min < arc_max) {
        arc_min = std::clamp(arc_min, 0.0f, 1.0f);
        arc_max = std::clamp(arc_max, 0.0f, 1.0f);
        const float ac = startAngle + (arc_min + arc_max) * 0.5f * sweep;
        const float ah = (arc_max - arc_min) * 0.5f * sweep;
        canvas.setColor(canvas.color(ApplauseKnobAccent).gradient().sample(0.0f).withAlpha(0.35f));
        canvas.arc(centerX - trackCenter, centerY - trackCenter, trackDiameter, arcThickness, ac, ah, true);
    }

    const float dotDiameter = arcThickness;
    const float dotTrackRadius = trackCenter - arcThickness * 0.5f;
    canvas.setColor(ApplauseKnobAccent);

    auto drawDot = [&](float v) {
        v = std::clamp(v, 0.0f, 1.0f);
        const float a = startAngle + v * sweep;
        const float cx = centerX + std::cos(a) * dotTrackRadius;
        const float cy = centerY + std::sin(a) * dotTrackRadius;
        canvas.circle(cx - dotDiameter * 0.5f, cy - dotDiameter * 0.5f, dotDiameter);
    };

    if (indicator_buf_.empty()) {
        drawDot(value_);
    } else {
        for (float v : indicator_buf_) drawDot(v);
        redraw();
    }

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

void Knob::mouseDown(const applause::MouseEvent& e) {
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

void Knob::mouseDrag(const applause::MouseEvent& e) {
    if (dragging_) {
        processDrag(e.position.y);
    }
}

void Knob::mouseUp(const applause::MouseEvent& e) {
    if (dragging_) {
        dragging_ = false;
        processDrag(e.position.y);
        onDragEnded.callback();
        if (!hovering_) glow_amount_.target(false);
        redraw();
    }
}

void Knob::mouseEnter(const applause::MouseEvent& e) {
    hovering_ = true;
    glow_amount_.target(true);
    redraw();
}

void Knob::mouseExit(const applause::MouseEvent& e) {
    hovering_ = false;
    if (!dragging_) glow_amount_.target(false);
    redraw();
}

bool Knob::mouseWheel(const applause::MouseEvent& e) {
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
