#pragma once

#include <applause/ui/ApplauseUI.h>

#include <functional>
#include <numbers>
#include <vector>

namespace applause {

/**
 * It's a Knob. Spin it around by dragging the mouse, or hovering and scrolling.
 *
 * The Knob doesn't do much on its own, unless you connect it to something.
 * If you want a plug-and-play Knob with a built-in label that
 * attaches to a Parameter, check out ParamKnob.
 */
class Knob : public applause::Frame {
public:
    using IndicatorProvider = std::function<bool(std::vector<float>&, float&, float&)>;

    static inline const ColorToken kBodyTop{"knob.bodyTop", 0xff333338};
    static inline const ColorToken kBodyBottom{"knob.bodyBottom", 0xff1a1a1e};
    static inline const ColorToken kBodyBorder{"knob.bodyBorder", 0xff444444};
    static inline const ColorToken kArcTrack{"knob.arcTrack", 0xff2a2a2e};
    static inline const ColorToken kAccent{"knob.accent", theme::kAccent};
    static inline const ColorToken kShadow{"knob.shadow", 0x88000000};

    static inline const ValueToken kArcThickness{"knob.arcThickness", 3.0f};
    static inline const ValueToken kTrackInset{"knob.trackInset", 1.5f};
    // In radians.
    static inline const ValueToken kSweep{"knob.sweep", 300.0f * std::numbers::pi_v<float> / 180.0f};
    // kAccent's opacity in the range arc from the indicator provider.
    static inline const ValueToken kRangeOpacity{"knob.rangeOpacity", 0.35f};

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
    // Return true while the indicators may change without notice (e.g. modulation is connected) so the knob keeps
    // redrawing every frame; return false to let it idle, and call redraw() when that changes.
    // This lets the knob ask a parent class (generally, the ParamKnob) if there are any sort of "modulation" sources
    // being applied to whatever destination this knob represents; then, the knob can draw the modulation as an overlay
    // however it sees fit.
    // We seperate the knob from the DSP/parameter/plugin side of things on purpose... knob is only visual; it doesn't
    // "know" anything about the plugin business.
    void setIndicatorProvider(IndicatorProvider provider);

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
    IndicatorProvider indicator_provider_;
    std::vector<float> indicator_buf_;
};

}  // namespace applause
