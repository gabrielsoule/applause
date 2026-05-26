#pragma once

#include <applause/ui/ApplauseUI.h>

#include <functional>
#include <vector>

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

    // Optional modulation hook: the provider fills `out` with normalized [0,1] positions to display,
    // and writes `arc_min`/`arc_max` in normalized space to draw a sub-arc on the track between those
    // angles. Leave `arc_min >= arc_max` (the initial sentinel) to suppress the arc.
    // This lets the knob ask a parent class (generally, the ParamKnob) if there are any sort of "modulation" sources
    // being applied to whatever destination this knob represents; then, the knob can draw the modulation as an overlay
    // however it sees fit.
    // We seperate the knob from the DSP/parameter/plugin side of things on purpose... knob is only visual; it doesn't
    // "know" anything about the plugin business.
    void setIndicatorProvider(
        std::function<void(std::vector<float>& dots, float& arc_min, float& arc_max)> provider);

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
    std::function<void(std::vector<float>&, float&, float&)> indicator_provider_;
    std::vector<float> indicator_buf_;
};

}  // namespace applause
