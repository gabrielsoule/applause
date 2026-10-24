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
    static inline const ColorToken kLine{"simpleCurve.line", theme::kAccent};
    static inline const ValueToken kLineWidth{"simpleCurve.lineWidth", 2.0f};
    // kLine's opacity in the fill under the curve.
    static inline const ValueToken kFillOpacity{"simpleCurve.fillOpacity", 0.19f};

    SimpleCurve();

    /// Copies uniformly spaced normalized samples, where 0 is bottom and 1 is top.
    void setSamples(std::span<const float> samples);
    void draw(applause::Canvas& canvas) override;

private:
    applause::GraphData source_data_;
    applause::GraphData data_;
};

/**
 * Renders a background panel with a customizable grid pattern. Looks nice when placed behind a curve!
 *
 * Curve not included.
 */
class PlotView : public applause::Frame {
public:
    static inline const ColorToken kBackground{"plotView.background", 0xff15151a};
    static inline const ColorToken kBorder{"plotView.border", 0xff2e2e34};
    static inline const ValueToken kRounding{"plotView.rounding", 4.0f};
    static inline const ValueToken kBorderWidth{"plotView.borderWidth", 1.0f};
    // kBorder's opacity in the grid lines.
    static inline const ValueToken kGridOpacity{"plotView.gridOpacity", 0.55f};

    PlotView();

    void setGridDivisions(int columns, int rows);

    void draw(applause::Canvas& canvas) override;

private:
    int columns_ = 4;
    int rows_ = 4;
};

}  // namespace applause
