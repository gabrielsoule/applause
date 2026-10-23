#pragma once

#include <applause/ui/ApplauseUI.h>

namespace applause {

/**
 * A UI slider with a circular thumb that moves along a track.
 */
class Slider : public applause::Frame {
public:
    static inline const ColorToken kThumbTop{"slider.thumbTop", 0xff333338};
    static inline const ColorToken kThumbBottom{"slider.thumbBottom", 0xff1a1a1e};
    static inline const ColorToken kThumbBorder{"slider.thumbBorder", 0xff444444};
    static inline const ColorToken kTrack{"slider.track", 0xff2a2a2e};
    static inline const ColorToken kAccent{"slider.accent", theme::kAccent};

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
