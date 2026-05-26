#pragma once

#include <applause/ui/ApplauseUI.h>

#include <applause/dsp/modulation/MSEGCurve.h>

namespace applause {

/// A reusable, context-agnostic MSEG curve editor.
/// Draws and allows interactive editing of an MSEGCurve. Knows nothing about
/// what the curve represents (LFO shape, envelope, etc.) — the parent component
/// provides background grid, axis labels, playhead, and semantic meaning.
/// Draws with a transparent background so the parent can render behind it.
class MSEGDisplay : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(MSEGDisplayLine);
    APPLAUSE_THEME_DEFINE_COLOR(MSEGDisplayFill);
    APPLAUSE_THEME_DEFINE_COLOR(MSEGDisplayPoint);
    APPLAUSE_THEME_DEFINE_COLOR(MSEGDisplayPointHover);
    APPLAUSE_THEME_DEFINE_COLOR(MSEGDisplayMidpoint);
    APPLAUSE_THEME_DEFINE_COLOR(MSEGDisplayMidpointHover);
    APPLAUSE_THEME_DEFINE_VALUE(MSEGDisplayLineWidth);
    APPLAUSE_THEME_DEFINE_VALUE(MSEGDisplayPointRadius);
    APPLAUSE_THEME_DEFINE_VALUE(MSEGDisplayMidpointRadius);

    explicit MSEGDisplay(MSEGCurve<>* curve = nullptr);

    void draw(applause::Canvas& canvas) override;

    void mouseDown(const applause::MouseEvent& e) override;
    void mouseDrag(const applause::MouseEvent& e) override;
    void mouseUp(const applause::MouseEvent& e) override;
    void mouseMove(const applause::MouseEvent& e) override;
    void mouseExit(const applause::MouseEvent& e) override;

    void setCurve(MSEGCurve<>* curve) { curve_ = curve; redraw(); }
    MSEGCurve<>* curve() const { return curve_; }

    void setYRange(float min, float max) { y_min_ = min; y_max_ = max; redraw(); }
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

    MSEGCurve<>* curve_ = nullptr;

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

} // namespace applause