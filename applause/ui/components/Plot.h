#pragma once

#include <applause/ui/ApplauseUI.h>

#include <span>

namespace applause {

/**
 * This component plots a curve. It uses Visage's fast curve rendering, which connects a list
 * of x-equidistant points into a line. If you want segments/curves/complex pathing, use the
 * Visage path system instead; it's a little slower, but more advanced.
 *
 * This component is mostly a wrapper around Canvas::graphLine, but provides some nice QoL goodies
 * like stroke-aware y-axis rescaling so that the line fits into the the entire component instead of
 * being cropped at the edges.
 */
class SimpleCurve : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTraceLine);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTraceFill);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseTraceLineWidth);

    SimpleCurve();

    /// Copies uniformly spaced normalized samples, where 0 is bottom and 1 is top.
    void setSamples(std::span<const float> samples);
    void draw(applause::Canvas& canvas) override;

private:
    visage::GraphData source_data_;
    visage::GraphData data_;
};

/**
 * Renders a background panel with a customizable grid pattern. Looks nice when placed behind a curve!
 *
 * Curve not included.
 */
class PlotView : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplausePlotBackground);
    APPLAUSE_THEME_DEFINE_COLOR(ApplausePlotGrid);
    APPLAUSE_THEME_DEFINE_COLOR(ApplausePlotBorder);
    APPLAUSE_THEME_DEFINE_VALUE(ApplausePlotRounding);
    APPLAUSE_THEME_DEFINE_VALUE(ApplausePlotBorderWidth);

    PlotView();

    void setGridDivisions(int columns, int rows);

    void draw(applause::Canvas& canvas) override;

private:
    int columns_ = 4;
    int rows_ = 4;
};

}  // namespace applause
