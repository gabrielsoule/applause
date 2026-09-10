#include "Plot.h"

#include <algorithm>

namespace applause {

APPLAUSE_THEME_IMPLEMENT_COLOR(SimpleCurve, ApplauseTraceLine, 0xff9966ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(SimpleCurve, ApplauseTraceFill, 0x309966ff);
APPLAUSE_THEME_IMPLEMENT_VALUE(SimpleCurve, ApplauseTraceLineWidth, 2.0f);

APPLAUSE_THEME_IMPLEMENT_COLOR(PlotView, ApplausePlotBackground, 0xff15151a);
APPLAUSE_THEME_IMPLEMENT_COLOR(PlotView, ApplausePlotGrid, 0x8c2e2e34);
APPLAUSE_THEME_IMPLEMENT_COLOR(PlotView, ApplausePlotBorder, 0xff2e2e34);
APPLAUSE_THEME_IMPLEMENT_VALUE(PlotView, ApplausePlotRounding, 4.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(PlotView, ApplausePlotBorderWidth, 1.0f);

SimpleCurve::SimpleCurve() { setIgnoresMouseEvents(true, false); }

void SimpleCurve::setSamples(std::span<const float> samples) {
    const int size = static_cast<int>(samples.size());
    source_data_.setNumPoints(size);
    data_.setNumPoints(size);
    for (int i = 0; i < size; ++i) source_data_[i] = 1.0f - samples[i];
    redraw();
}

void SimpleCurve::draw(applause::Canvas& canvas) {
    if (source_data_.numPoints() < 2 || width() <= 0.0f || height() <= 0.0f ||
        canvas.totallyClamped())
        return;

    const float line_width = std::max(0.0f, canvas.value(ApplauseTraceLineWidth));
    // Visage's graph coordinates include one native pixel of edge antialiasing.
    const float pixel = 1.0f / canvas.dpiScale();
    const float inset = std::min((line_width * 0.5f + pixel) / (height() + pixel), 0.5f);
    const float scale = 1.0f - 2.0f * inset;
    for (int i = 0; i < data_.numPoints(); ++i) data_[i] = inset + source_data_[i] * scale;

    const applause::Color fill_color = canvas.color(ApplauseTraceFill).gradient().sample(0.0f);
    canvas.setColor(applause::Brush::vertical(fill_color, fill_color.withAlpha(0.0f)));
    canvas.graphFill(data_, 0.0f, 0.0f, width(), height(), 1.0f);

    canvas.setColor(ApplauseTraceLine);
    canvas.graphLine(data_, 0.0f, 0.0f, width(), height(), line_width);
}

PlotView::PlotView() { setIgnoresMouseEvents(true, true); }

void PlotView::setGridDivisions(int columns, int rows) {
    columns_ = std::max(1, columns);
    rows_ = std::max(1, rows);
    redraw();
}

void PlotView::draw(applause::Canvas& canvas) {
    const float rounding = canvas.value(ApplausePlotRounding);

    canvas.setColor(ApplausePlotBackground);
    if (rounding > 0.0f)
        canvas.roundedRectangle(0.0f, 0.0f, width(), height(), rounding);
    else
        canvas.fill(0.0f, 0.0f, width(), height());

    canvas.setColor(ApplausePlotGrid);
    for (int column = 1; column < columns_; ++column) {
        const float x = width() * column / columns_;
        canvas.segment(x, 0.0f, x, height(), 1.0f, false);
    }
    for (int row = 1; row < rows_; ++row) {
        const float y = height() * row / rows_;
        canvas.segment(0.0f, y, width(), y, 1.0f, false);
    }

    const float border_width = canvas.value(ApplausePlotBorderWidth);
    if (border_width > 0.0f) {
        canvas.setColor(ApplausePlotBorder);
        if (rounding > 0.0f)
            canvas.roundedRectangleBorder(0.0f, 0.0f, width(), height(), rounding, border_width);
        else
            canvas.rectangleBorder(0.0f, 0.0f, width(), height(), border_width);
    }
}

}  // namespace applause
