#pragma once

#include <visage_graphics/animation.h>
#include <visage_ui/frame.h>

namespace applause {

/**
 * A simple slider widget.
 */
class Slider : public visage::Frame {
public:
    Slider();

    void draw(visage::Canvas& canvas) override;
    void resized() override;
    void mouseDown(const visage::MouseEvent& event) override;
    void mouseDrag(const visage::MouseEvent& event) override;
    void mouseUp(const visage::MouseEvent& event) override;
    void mouseEnter(const visage::MouseEvent& event) override;
    void mouseExit(const visage::MouseEvent& event) override;
    bool mouseWheel(const visage::MouseEvent& event) override;

    void setValue(float value);
    void setBipolar(bool bipolar);
    bool isBipolar() const { return bipolar_; }

    visage::CallbackList<void(float)> on_value_changed;
    visage::CallbackList<void()> on_drag_started;
    visage::CallbackList<void()> on_drag_ended;

private:
    void processDrag(float rawDragPos);
    bool dragging_ = false;
    bool hovering_ = false;
    bool bipolar_ = false;
    float value_ = 0;  // [0, 1] or [-1, 1] when bipolar
    visage::Animation<float> hover_amount_;

    static constexpr float kWheelSensitivity = 0.01f;
};

}  // namespace applause
