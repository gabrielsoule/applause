#include "Slider.h"

#include <algorithm>
#include <cmath>

#include "applause/util/DebugHelpers.h"

namespace applause {

Slider::Slider() {
    hover_amount_.setTargetValue(1.0f);
    hover_amount_.setSourceValue(0.0f);  // Set initial value to 0
    hover_amount_.setAnimationTime(150);
}

void Slider::resized() {}

void Slider::mouseDown(const visage::MouseEvent& event) {
    dragging_ = true;
    hover_amount_.target(true);  // Keep animation at max during drag
    on_drag_started.callback();
    processDrag(event.position.x);
    redraw();
}

void Slider::mouseDrag(const visage::MouseEvent& event) {
    processDrag(event.position.x);
}

void Slider::mouseUp(const visage::MouseEvent& event) {
    dragging_ = false;
    on_drag_ended.callback();
    processDrag(event.position.x);
    // If not hovering anymore, animate back to normal
    if (!hovering_) {
        hover_amount_.target(false);
    }
    redraw();
}

bool Slider::mouseWheel(const visage::MouseEvent& event) {
    float delta = -event.precise_wheel_delta_y * kWheelSensitivity;
    float minVal = bipolar_ ? -1.0f : 0.0f;
    float newValue = std::clamp(value_ + delta, minVal, 1.0f);

    // Only process if the value actually changed
    if (newValue != value_) {
        on_drag_started.callback();

        value_ = newValue;
        on_value_changed.callback(value_);
        redraw();

        on_drag_ended.callback();

        return true;
    }

    return false;  // Event was not handled
}

void Slider::setValue(float value) {
    if (bipolar_) {
        value_ = std::clamp(value, -1.0f, 1.0f);
    } else {
        value_ = std::clamp(value, 0.0f, 1.0f);
    }
    redraw();
}

void Slider::setBipolar(bool bipolar) {
    if (bipolar_ != bipolar) {
        bipolar_ = bipolar;
        value_ = 0.0f;  // Reset to neutral position
        redraw();
    }
}

void Slider::processDrag(float rawDragPos) {
    if (bipolar_) {
        // Map [0, width] to [-1, 1]
        value_ = std::clamp((rawDragPos / width()) * 2.0f - 1.0f, -1.0f, 1.0f);
    } else {
        value_ = std::clamp(rawDragPos / width(), 0.0f, 1.0f);
    }
    on_value_changed.callback(value_);
    redraw();
}

void Slider::mouseEnter(const visage::MouseEvent& event) {
    hovering_ = true;
    hover_amount_.target(true);
    redraw();
}

void Slider::mouseExit(const visage::MouseEvent& event) {
    hovering_ = false;
    if (!dragging_) {
        hover_amount_.target(false);
    }
    redraw();
}

void Slider::draw(visage::Canvas& canvas) {
    // Update animation and determine border thickness
    hover_amount_.update();
    float animValue = hover_amount_.value();
    float borderThickness =
        2.0f + (animValue * 2.0f);  // Animates from 2.0f to 4.0f

    if (hover_amount_.isAnimating()) redraw();

    canvas.setColor(0xFFFFFFFF);
    canvas.rectangleBorder(0, 0, width(), height(), borderThickness);

    if (bipolar_) {
        float centerX = width() / 2.0f;
        float fillWidth = std::abs(value_) * centerX;
        if (value_ >= 0.0f) {
            // Fill from center to right
            canvas.fill(centerX, 0, fillWidth, height());
        } else {
            // Fill from center to left
            canvas.fill(centerX - fillWidth, 0, fillWidth, height());
        }
    } else {
        canvas.fill(0, 0, value_ * width(), height());
    }
}

}  // namespace applause
