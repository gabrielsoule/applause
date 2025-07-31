#pragma once

#include <visage_ui/frame.h>

namespace applause {

class Knob : public visage::Frame {
public:
    Knob();
    ~Knob() override = default;

    // Value management
    void setValue(float value);
    float getValue() const { return value_; }

    visage::CallbackList<void(float)> onValueChanged;
    visage::CallbackList<void()> onDragStarted;
    visage::CallbackList<void()> onDragEnded;

protected:
    void draw(visage::Canvas& canvas) override;
    void mouseDown(const visage::MouseEvent& e) override;
    void mouseDrag(const visage::MouseEvent& e) override;
    void mouseUp(const visage::MouseEvent& e) override;
    bool mouseWheel(const visage::MouseEvent& e) override;

private:
    void processDrag(float mouseY);
    
    float value_ = 0.0f;  // Normalized value [0, 1]
    bool dragging_ = false;
    float drag_start_y_ = 0.0f;
    float drag_start_value_ = 0.0f;
    
    static constexpr float kDragSensitivity = 0.005f;
    static constexpr float kWheelSensitivity = 0.015f;
};

} // namespace applause