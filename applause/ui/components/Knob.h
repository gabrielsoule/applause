#pragma once

#include <applause/ui/ApplauseUI.h>

namespace applause {

/**
 * A simple Knob. Spin it around by dragging the mouse, or hovering and scrolling.
 *
 * The Knob doesn't do much on its own. If you want a plug-and-play Knob with a built-in label that
 * attaches to a Parameter, check out ParamKnob.
 */
class Knob : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseKnobBodyTop);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseKnobBodyBottom);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseKnobBodyBorder);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseKnobArcTrack);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseKnobAccent);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseKnobShadow);

    APPLAUSE_THEME_DEFINE_VALUE(ApplauseKnobArcThickness);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseKnobTrackInset);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseKnobSweep);

    Knob();
    ~Knob() override = default;

    void setValue(float value);
    float getValue() const { return value_; }

    void setDefaultValue(float value);
    float getDefaultValue() const { return default_value_; }

    applause::CallbackList<void(float)> onValueChanged;
    applause::CallbackList<void()> onDragStarted;
    applause::CallbackList<void()> onDragEnded;

    void setDragSensitivity(float sensitivity) { drag_sensitivity_ = sensitivity; }
    void setWheelSensitivity(float sensitivity) { wheel_sensitivity_ = sensitivity; }

protected:
    void draw(applause::Canvas& canvas) override;
    void mouseDown(const applause::MouseEvent& e) override;
    void mouseDrag(const applause::MouseEvent& e) override;
    void mouseUp(const applause::MouseEvent& e) override;
    void mouseEnter(const applause::MouseEvent& e) override;
    void mouseExit(const applause::MouseEvent& e) override;
    bool mouseWheel(const applause::MouseEvent& e) override;

private:
    void processDrag(float mouseY);

    float value_ = 0.0f;  // normalized value between 0 and 1, not bipolar
    float default_value_ = 0.0f;  // normalized [0,1]; rim arc draws from here, double-click resets here
    bool dragging_ = false;
    bool hovering_ = false;
    float drag_start_y_ = 0.0f;
    float drag_start_value_ = 0.0f;
    float drag_sensitivity_ = 0.005f;
    float wheel_sensitivity_ = 0.015f;
    applause::Animation<float> glow_amount_;
};

}  // namespace applause
