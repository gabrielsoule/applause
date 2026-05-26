#pragma once

#include <applause/ui/ApplauseUI.h>

namespace applause {

/**
 * A horizontal slider with a circular thumb and track, styled like a classic JUCE slider.
 * The usable pixel range is inset by the thumb radius on both sides so the thumb
 * never extends past the component bounds.
 */
class Slider : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSliderThumbTop);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSliderThumbBottom);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSliderThumbBorder);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSliderTrack);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseSliderAccent);

    Slider();

    void draw(applause::Canvas& canvas) override;
    void resized() override;
    void mouseDown(const applause::MouseEvent& event) override;
    void mouseDrag(const applause::MouseEvent& event) override;
    void mouseUp(const applause::MouseEvent& event) override;
    void mouseEnter(const applause::MouseEvent& event) override;
    void mouseExit(const applause::MouseEvent& event) override;
    bool mouseWheel(const applause::MouseEvent& event) override;

    void setValue(float value);
    float getValue() const { return value_; }

    void setDefaultValue(float value);
    float getDefaultValue() const { return default_value_; }

    void setBipolar(bool bipolar);
    bool isBipolar() const { return bipolar_; }

    void setActive(bool active) {
        active_ = active;
        redraw();
    }
    bool isActive() const { return active_; }

    applause::CallbackList<void(float)> on_value_changed;
    applause::CallbackList<void()> on_drag_started;
    applause::CallbackList<void()> on_drag_ended;

private:
    void processDrag(float raw_drag_pos);

    bool active_ = true;
    bool dragging_ = false;
    bool hovering_ = false;
    bool bipolar_ = false;
    float value_ = 0;  // [0, 1] or [-1, 1] when bipolar
    float default_value_ = 0;  // [0, 1] or [-1, 1] when bipolar
    applause::Animation<float> glow_amount_;

    static constexpr float kWheelSensitivity = 0.01f;
    static constexpr float kThumbRadius = 10.0f;
    static constexpr float kThumbDiameter = kThumbRadius * 2.0f;
    static constexpr float kTrackHeight = 5.0f;
};

}  // namespace applause
