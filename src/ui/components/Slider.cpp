#include "Slider.h"
#include "util/DebugHelpers.h"
#include <algorithm>

namespace applause {

void Slider::resized()
{
}

void Slider::mouseDown(const visage::MouseEvent& event)
{
    dragging_ = true;
    on_drag_started.callback();
    processDrag(event.position.x);
}

void Slider::mouseDrag(const visage::MouseEvent& event)
{
    processDrag(event.position.x);
}

void Slider::mouseUp(const visage::MouseEvent& event)
{
    dragging_ = false;
    on_drag_ended.callback();
    processDrag(event.position.x);
}

bool Slider::mouseWheel(const visage::MouseEvent& event)
{
    float delta = -event.precise_wheel_delta_y * kWheelSensitivity;
    float newValue = std::clamp(value_ + delta, 0.0f, 1.0f);
    
    // Only process if the value actually changed
    if (newValue != value_) {
        on_drag_started.callback();
        
        value_ = newValue;
        on_value_changed.callback(value_);
        redraw();
        
        on_drag_ended.callback();
        
        return true;
    }
    
    return false; // Event was not handled
}

void Slider::setValue(float value)
{
    value_ = std::clamp(value, 0.0f, 1.0f);
    redraw();
}

void Slider::processDrag(float rawDragPos)
{
    value_ = std::clamp(rawDragPos / width(), 0.0f, 1.0f);
    on_value_changed.callback(value_);
    redraw();
}


void Slider::draw(visage::Canvas& canvas)
{
    canvas.setColor(0xFFFFFFFF);
    canvas.rectangleBorder(0, 0, width(), height(), 2);
    canvas.fill(0, 0, value_ * width(), height());
}

} // namespace applause


