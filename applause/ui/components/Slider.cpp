#include "Slider.h"

#include <algorithm>
#include <cmath>

#include <visage_graphics/theme.h>

namespace applause {

VISAGE_THEME_IMPLEMENT_COLOR(Slider, ApplauseSliderThumbTop, 0xFF333338);
VISAGE_THEME_IMPLEMENT_COLOR(Slider, ApplauseSliderThumbBottom, 0xFF1a1a1e);
VISAGE_THEME_IMPLEMENT_COLOR(Slider, ApplauseSliderThumbBorder, 0xFF444444);
VISAGE_THEME_IMPLEMENT_COLOR(Slider, ApplauseSliderTrack, 0xFF2a2a2e);
VISAGE_THEME_IMPLEMENT_COLOR(Slider, ApplauseSliderAccent, 0xff9966ff);

Slider::Slider() {
    glow_amount_.setTargetValue(1.0f);
    glow_amount_.setSourceValue(0.0f);
    glow_amount_.setAnimationTime(150);
}

void Slider::resized() {}

void Slider::mouseDown(const visage::MouseEvent& event) {
    if (!active_) return;
    dragging_ = true;
    glow_amount_.target(true);
    on_drag_started.callback();
    processDrag(event.position.x);
    redraw();
}

void Slider::mouseDrag(const visage::MouseEvent& event) {
    if (!active_) return;
    processDrag(event.position.x);
}

void Slider::mouseUp(const visage::MouseEvent& event) {
    if (!active_) return;
    dragging_ = false;
    on_drag_ended.callback();
    processDrag(event.position.x);
    if (!hovering_)
        glow_amount_.target(false);
    redraw();
}

bool Slider::mouseWheel(const visage::MouseEvent& event) {
    if (!active_) return false;
    float delta = -event.precise_wheel_delta_y * kWheelSensitivity;
    float minVal = bipolar_ ? -1.0f : 0.0f;
    float newValue = std::clamp(value_ + delta, minVal, 1.0f);

    if (newValue != value_) {
        on_drag_started.callback();
        value_ = newValue;
        on_value_changed.callback(value_);
        redraw();
        on_drag_ended.callback();
        return true;
    }
    return false;
}

void Slider::setValue(float value) {
    if (bipolar_)
        value_ = std::clamp(value, -1.0f, 1.0f);
    else
        value_ = std::clamp(value, 0.0f, 1.0f);
    redraw();
}

void Slider::setBipolar(bool bipolar) {
    if (bipolar_ != bipolar) {
        bipolar_ = bipolar;
        value_ = 0.0f;
        redraw();
    }
}

void Slider::processDrag(float raw_drag_pos) {
    float usable = width() - kThumbDiameter;

    if (usable <= 0.0f) {
        value_ = 0.0f;
    } else if (bipolar_) {
        float normalized = (raw_drag_pos - kThumbRadius) / usable;
        value_ = std::clamp(normalized * 2.0f - 1.0f, -1.0f, 1.0f);
    } else {
        value_ = std::clamp((raw_drag_pos - kThumbRadius) / usable, 0.0f, 1.0f);
    }

    on_value_changed.callback(value_);
    redraw();
}

void Slider::mouseEnter(const visage::MouseEvent& event) {
    if (!active_) return;
    hovering_ = true;
    glow_amount_.target(true);
    redraw();
}

void Slider::mouseExit(const visage::MouseEvent& event) {
    if (!active_) return;
    hovering_ = false;
    if (!dragging_)
        glow_amount_.target(false);
    redraw();
}

void Slider::draw(visage::Canvas& canvas) {
    auto sample = [&](auto id) { return canvas.color(id).gradient().sample(0.0f); };

    float usable = width() - kThumbDiameter;
    float trackY = (height() - kTrackHeight) * 0.5f;
    float trackRounding = kTrackHeight * 0.5f;
    float centerY = height() * 0.5f;

    // Thumb center X from value
    float tcx;
    if (bipolar_) {
        float halfUsable = usable * 0.5f;
        tcx = width() * 0.5f + value_ * halfUsable;
    } else {
        tcx = kThumbRadius + value_ * usable;
    }

    float thumbX = tcx - kThumbRadius;
    float thumbY = centerY - kThumbRadius;

    if (!active_) {
        constexpr float kInactiveDim = 0.2f;
        visage::Color black(0xff000000);

        canvas.setColor(sample(ApplauseSliderTrack).interpolateWith(black, kInactiveDim));
        canvas.roundedRectangle(0, trackY, width(), kTrackHeight, trackRounding);

        if (bipolar_) {
            float centerX = width() * 0.5f;
            float fillW = std::abs(tcx - centerX);
            if (fillW > 0.0f) {
                canvas.setColor(sample(ApplauseSliderAccent).interpolateWith(black, kInactiveDim));
                canvas.roundedRectangle(value_ > 0.0f ? centerX : tcx, trackY, fillW, kTrackHeight, trackRounding);
            }
        } else if (value_ > 0.0f) {
            canvas.setColor(sample(ApplauseSliderAccent).interpolateWith(black, kInactiveDim));
            canvas.roundedRectangle(0, trackY, tcx, kTrackHeight, trackRounding);
        }

        canvas.setColor(visage::Brush::vertical(
            sample(ApplauseSliderThumbTop).interpolateWith(black, kInactiveDim),
            sample(ApplauseSliderThumbBottom).interpolateWith(black, kInactiveDim)));
        canvas.circle(thumbX, thumbY, kThumbDiameter);

        canvas.setColor(sample(ApplauseSliderThumbBorder).interpolateWith(black, kInactiveDim));
        canvas.ring(thumbX, thumbY, kThumbDiameter, 1.0f);
        return;
    }

    // Track background
    canvas.setColor(ApplauseSliderTrack);
    canvas.roundedRectangle(0, trackY, width(), kTrackHeight, trackRounding);

    // Accent fill
    if (bipolar_) {
        float centerX = width() * 0.5f;
        if (value_ > 0.0f) {
            float fillW = tcx - centerX;
            if (fillW > 0.0f) {
                canvas.setColor(ApplauseSliderAccent);
                canvas.roundedRectangle(centerX, trackY, fillW, kTrackHeight, trackRounding);
            }
        } else if (value_ < 0.0f) {
            float fillW = centerX - tcx;
            if (fillW > 0.0f) {
                canvas.setColor(ApplauseSliderAccent);
                canvas.roundedRectangle(tcx, trackY, fillW, kTrackHeight, trackRounding);
            }
        }
    } else {
        if (value_ > 0.0f) {
            canvas.setColor(ApplauseSliderAccent);
            canvas.roundedRectangle(0, trackY, tcx, kTrackHeight, trackRounding);
        }
    }

    // Thumb drop shadow
    float shadowOffset = 3.0f;
    float shadowPad = 2.0f;
    float shadowDiameter = kThumbDiameter + shadowPad * 2.0f;
    canvas.setColor(0x88000000);
    canvas.fadeCircle(tcx - shadowDiameter * 0.5f,
                      centerY - shadowDiameter * 0.5f + shadowOffset,
                      shadowDiameter, 8.0f);

    // Thumb body (vertical gradient)
    canvas.setColor(visage::Brush::vertical(
        sample(ApplauseSliderThumbTop), sample(ApplauseSliderThumbBottom)));
    canvas.circle(thumbX, thumbY, kThumbDiameter);

    // Thumb border ring
    canvas.setColor(ApplauseSliderThumbBorder);
    canvas.ring(thumbX, thumbY, kThumbDiameter, 1.0f);

    // Animated glow overlay
    glow_amount_.update();
    float glowAlpha = glow_amount_.value();
    if (glowAlpha > 0.0f) {
        visage::Color accent = sample(ApplauseSliderAccent);
        visage::Gradient glow;
        glow.addColorStop(accent.withAlpha(glowAlpha * 0.35f), 0.0f);
        glow.addColorStop(accent.withAlpha(glowAlpha * 0.15f), 0.6f);
        glow.addColorStop(accent.withAlpha(glowAlpha * 0.10f), 1.0f);
        visage::Point center = {tcx, centerY};
        canvas.setColor(visage::Brush::radial(glow, center, kThumbRadius, kThumbRadius));
        canvas.circle(thumbX, thumbY, kThumbDiameter);
    }
    if (glow_amount_.isAnimating()) redraw();
}

}  // namespace applause
