#pragma once

#include <applause/ui/ApplauseUI.h>

#include <applause/dsp/modulation/MSEGCurve.h>

namespace applause {

/// A reusable, context-agnostic MSEG curve editor.
/// Draws and allows interactive editing of an MSEGCurve.
/// Draws with a transparent background so the parent can render behind it.
class MSEGDisplay : public applause::Frame {
public:
    static inline const ColorToken kLine{"msegDisplay.line", theme::kAccent};
    static inline const ColorToken kPoint{"msegDisplay.point", 0xffdddddd};
    static inline const ColorToken kPointHover{"msegDisplay.pointHover", 0xffffffff};
    static inline const ColorToken kMidpoint{"msegDisplay.midpoint", 0x99aaaaaa};
    static inline const ColorToken kMidpointHover{"msegDisplay.midpointHover", 0xccdddddd};
    static inline const ValueToken kLineWidth{"msegDisplay.lineWidth", 2.0f};
    // kLine's opacity in the fill under the curve.
    static inline const ValueToken kFillOpacity{"msegDisplay.fillOpacity", 0.19f};
    static inline const ValueToken kPointRadius{"msegDisplay.pointRadius", 5.0f};
    static inline const ValueToken kMidpointRadius{"msegDisplay.midpointRadius", 3.5f};

    explicit MSEGDisplay(MSEGCurve<>* curve = nullptr);

    void draw(applause::Canvas& canvas) override;
    void resized() override { handles_.setBounds(localBounds()); }

    void mouseDown(const applause::MouseEvent& e) override;
    void mouseDrag(const applause::MouseEvent& e) override;
    void mouseUp(const applause::MouseEvent& e) override;
    void mouseMove(const applause::MouseEvent& e) override;
    void mouseExit(const applause::MouseEvent& e) override;

    void setCurve(MSEGCurve<>* curve) {
        curve_ = curve;
        redraw();
    }
    MSEGCurve<>* curve() const { return curve_; }

    void setYRange(float min, float max) {
        y_min_ = min;
        y_max_ = max;
        redraw();
    }
    void allowAddRemovePoints(bool enabled) { point_editing_enabled_ = enabled; }

    applause::CallbackList<void()> on_curve_changed;

private:
    // Coordinate mapping; normalized curve space <-> screen space
    float curveXToScreen(float cx) const;
    float curveYToScreen(float cy) const;
    float screenXToCurve(float sx) const;
    float screenYToCurve(float sy) const;

    // Returns index of point near (sx, sy), or -1
    int hitTestPoint(float sx, float sy) const;
    // Returns segment index whose midpoint handle is near (sx, sy), or -1
    int hitTestMidpoint(float sx, float sy) const;

    void drawHandles(applause::Canvas& canvas);

    MSEGCurve<>* curve_ = nullptr;
    // Handles draw in their own frame so hover highlights don't re-record the curve.
    applause::Frame handles_;

    // Display settings
    float y_min_ = 0.0f;
    float y_max_ = 1.0f;

    // Interaction state
    int hovered_point_ = -1;
    int hovered_midpoint_ = -1;
    int dragged_point_ = -1;
    int dragged_segment_ = -1;
    bool point_editing_enabled_ = true;

    std::vector<applause::Point> samples_;
};

}  // namespace applause
