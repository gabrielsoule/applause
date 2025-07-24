#include "Knob.h"
#include "util/DebugHelpers.h"
#include <cmath>
#include <algorithm>

namespace applause {

Knob::Knob() {
    // Mouse events are accepted by default in Visage
}

void Knob::setValue(float value) {
    value_ = std::clamp(value, 0.0f, 1.0f);
    redraw();
}

void Knob::draw(visage::Canvas& canvas) {
    // Calculate dimensions - use the smaller of width/height for a square knob
    float size = std::min(width(), height());
    float centerX = width() * 0.5f;
    float centerY = height() * 0.5f;
    float radius = size * 0.5f;
    
    // Draw white arc from 7:00 to 5:00
    canvas.setColor(0xFF828282);  // White
    
    // Arc parameters:
    // - Center at 270° (12:00 position)
    // - Half-span of 150° (for total arc of 300°)
    // Arc extends from center-150° to center+150° = 120° to 420° = 7:00 to 5:00
    float centerRadians = 270.0f * (static_cast<float>(M_PI) / 180.0f);  // 3π/2
    float spanRadians = 150.0f * (static_cast<float>(M_PI) / 180.0f);    // 5π/6 (half of 300°)
    
    canvas.arc(centerX - radius, centerY - radius, size, 2.0f, 
               centerRadians, spanRadians, true);
    
    // Draw progress arc from 7:00 to current position
    if (value_ > 0.0f) {
        canvas.setColor(0xFFFFFFFF);  // Bright blue
        
        // Progress arc: starts at 120° (7:00) and spans to current position
        // Center of progress arc = 120° + (value * 300°) / 2
        // Half-span of progress arc = (value * 300°) / 2
        float progressCenterDeg = 120.0f + (value_ * 150.0f);
        float progressSpanDeg = value_ * 150.0f;
        
        float progressCenterRad = progressCenterDeg * (static_cast<float>(M_PI) / 180.0f);
        float progressSpanRad = progressSpanDeg * (static_cast<float>(M_PI) / 180.0f);
        
        canvas.arc(centerX - radius, centerY - radius, size, 2.0f,
                   progressCenterRad, progressSpanRad, true);
    }
    
    // Draw small center circle
    float centerDotRadius = radius * 0.15f;
    canvas.setColor(0xFFFFFFFF);  // White
    canvas.circle(centerX - centerDotRadius, centerY - centerDotRadius, centerDotRadius * 2.0f);
    
    // Calculate line position based on value
    // In screen coordinates where cos(0°)=right, sin(90°)=down:
    // Start at 7:00 (120°), sweep 300° clockwise to 5:00 (420°)
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
    onDragStarted.callback();
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
    }
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
    // Calculate value change based on vertical mouse movement
    float deltaY = drag_start_y_ - mouseY;  // Inverted so up = increase
    float deltaValue = deltaY * kDragSensitivity;
    
    float newValue = drag_start_value_ + deltaValue;
    newValue = std::clamp(newValue, 0.0f, 1.0f);
    
    if (newValue != value_) {
        value_ = newValue;
        onValueChanged.callback(value_);
        redraw();
    }
}

} // namespace applause