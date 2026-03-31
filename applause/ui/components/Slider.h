#pragma once

#include <visage_graphics/animation.h>
#include <visage_graphics/theme.h>
#include <visage_ui/frame.h>

namespace applause {

/**
 * A horizontal slider with a circular thumb and track, styled like a classic JUCE slider.
 * The usable pixel range is inset by the thumb radius on both sides so the thumb
 * never extends past the component bounds.
 */
class Slider : public visage::Frame {
public:
    VISAGE_THEME_DEFINE_COLOR(ApplauseSliderThumbTop);
    VISAGE_THEME_DEFINE_COLOR(ApplauseSliderThumbBottom);
    VISAGE_THEME_DEFINE_COLOR(ApplauseSliderThumbBorder);
    VISAGE_THEME_DEFINE_COLOR(ApplauseSliderTrack);
    VISAGE_THEME_DEFINE_COLOR(ApplauseSliderAccent);

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
    void processDrag(float raw_drag_pos);

    bool dragging_ = false;
    bool hovering_ = false;
    bool bipolar_ = false;
    float value_ = 0;  // [0, 1] or [-1, 1] when bipolar
    visage::Animation<float> glow_amount_;

    static constexpr float kWheelSensitivity = 0.01f;
    static constexpr float kThumbRadius = 10.0f;
    static constexpr float kThumbDiameter = kThumbRadius * 2.0f;
    static constexpr float kTrackHeight = 5.0f;
};

}  // namespace applause
