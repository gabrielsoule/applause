#pragma once

#include <visage_graphics/animation.h>
#include <visage_graphics/theme.h>
#include <visage_ui/frame.h>

namespace applause {

/**
 * A simple Knob. Spin it around by dragging the mouse, or hovering and scrolling.
 *
 * The Knob doesn't do much on its own. If you want a plug-and-play Knob with a built-in label that
 * attaches to a Parameter, check out ParamKnob.
 */
class Knob : public visage::Frame {
public:
    VISAGE_THEME_DEFINE_COLOR(ApplauseKnobBodyTop);
    VISAGE_THEME_DEFINE_COLOR(ApplauseKnobBodyBottom);
    VISAGE_THEME_DEFINE_COLOR(ApplauseKnobBodyBorder);
    VISAGE_THEME_DEFINE_COLOR(ApplauseKnobArcTrack);
    VISAGE_THEME_DEFINE_COLOR(ApplauseKnobAccent);

    Knob();
    ~Knob() override = default;

    void setValue(float value);
    float getValue() const { return value_; }

    visage::CallbackList<void(float)> onValueChanged;
    visage::CallbackList<void()> onDragStarted;
    visage::CallbackList<void()> onDragEnded;

    void setDragSensitivity(float sensitivity) { drag_sensitivity_ = sensitivity; }
    void setWheelSensitivity(float sensitivity) { wheel_sensitivity_ = sensitivity; }
protected:
    void draw(visage::Canvas& canvas) override;
    void mouseDown(const visage::MouseEvent& e) override;
    void mouseDrag(const visage::MouseEvent& e) override;
    void mouseUp(const visage::MouseEvent& e) override;
    void mouseEnter(const visage::MouseEvent& e) override;
    void mouseExit(const visage::MouseEvent& e) override;
    bool mouseWheel(const visage::MouseEvent& e) override;

private:
    void processDrag(float mouseY);

    float value_ = 0.0f;  // normalized value between 0 and 1, not bipolar
    bool dragging_ = false;
    bool hovering_ = false;
    float drag_start_y_ = 0.0f;
    float drag_start_value_ = 0.0f;
    float drag_sensitivity_ = 0.005f;
    float wheel_sensitivity_ = 0.015f;
    float sweep_ = 300.0f * (static_cast<float>(M_PI) / 180.0f);
    visage::Animation<float> glow_amount_;
};

}  // namespace applause
