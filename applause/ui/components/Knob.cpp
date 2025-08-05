#include "Knob.h"
#include "applause/util/DebugHelpers.h"
#include <cmath>
#include <algorithm>

namespace applause {

Knob::Knob() {
    hover_amount_.setTargetValue(1.0f);
    hover_amount_.setSourceValue(0.0f);  // Set initial value to 0
    hover_amount_.setAnimationTime(150);
}

void Knob::setValue(float value) {
    value_ = std::clamp(value, 0.0f, 1.0f);
    redraw();
}

void Knob::draw(visage::Canvas& canvas) {
    float size = std::min(width(), height());
    float centerX = width() * 0.5f;
    float centerY = height() * 0.5f;
    float radius = size * 0.5f;
    
    // Update animation and determine arc thickness
    hover_amount_.update();
    float animValue = hover_amount_.value();
    float arcThickness = 2.0f + (animValue * 2.0f);

    if (hover_amount_.isAnimating())
        redraw();
    canvas.setColor(0xFF828282);

    float centerRadians = 270.0f * (static_cast<float>(M_PI) / 180.0f);
    float spanRadians = 150.0f * (static_cast<float>(M_PI) / 180.0f);
    
    canvas.arc(centerX - radius, centerY - radius, size, arcThickness, 
               centerRadians, spanRadians, true);

    if (value_ > 0.0f) {
        canvas.setColor(0xFFFFFFFF);  // Bright blue

        float progressCenterDeg = 120.0f + (value_ * 150.0f);
        float progressSpanDeg = value_ * 150.0f;
        
        float progressCenterRad = progressCenterDeg * (static_cast<float>(M_PI) / 180.0f);
        float progressSpanRad = progressSpanDeg * (static_cast<float>(M_PI) / 180.0f);
        
        canvas.arc(centerX - radius, centerY - radius, size, arcThickness,
                   progressCenterRad, progressSpanRad, true);
    }
    
    // Draw small center circle
    float centerDotRadius = radius * 0.15f;
    canvas.setColor(0xFFFFFFFF);  // White
    canvas.circle(centerX - centerDotRadius, centerY - centerDotRadius, centerDotRadius * 2.0f);
    
    // Calculate line position based on value
    float angle = 120.0f + (value_ * 300.0f);
    float angleRad = angle * (static_cast<float>(M_PI) / 180.0f);
    
    // Calculate line end point (from center to edge)
    float lineEndX = centerX + static_cast<float>(std::cos(angleRad)) * (radius * 0.6f);
    float lineEndY = centerY + static_cast<float>(std::sin(angleRad)) * (radius * 0.6f);
    
    // Draw indicator line
    canvas.setColor(0xFFFFFFFF);  // White
    canvas.segment(centerX, centerY, lineEndX, lineEndY, 2.0f, true);
}

void Knob::mouseDown(const visage::MouseEvent& e) {
    dragging_ = true;
    drag_start_y_ = e.position.y;
    drag_start_value_ = value_;
    hover_amount_.target(true);  // Keep animation at max during drag
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
        // If not hovering anymore, animate back to normal
        if (!hovering_) {
            hover_amount_.target(false);
        }
        redraw();
    }
}

void Knob::mouseEnter(const visage::MouseEvent& e) {
    hovering_ = true;
    hover_amount_.target(true);
    redraw();
}

void Knob::mouseExit(const visage::MouseEvent& e) {
    hovering_ = false;
    if (!dragging_) {
        hover_amount_.target(false);
    }
    redraw();
}

bool Knob::mouseWheel(const visage::MouseEvent& e) {
    float delta = -e.precise_wheel_delta_y * kWheelSensitivity;
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
    const float delta_value = delta_y * kDragSensitivity;
    
    const float newValue = std::clamp(drag_start_value_ + delta_value, 0.0f, 1.0f);
    
    if (newValue != value_) {
        value_ = newValue;
        onValueChanged.callback(value_);
        redraw();
    }
}

} // namespace applause