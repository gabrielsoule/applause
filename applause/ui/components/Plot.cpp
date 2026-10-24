#include "Plot.h"

#include <algorithm>

namespace applause {

SimpleCurve::SimpleCurve() { setIgnoresMouseEvents(true, false); }

void SimpleCurve::setSamples(std::span<const float> samples) {
    const int size = static_cast<int>(samples.size());
    source_data_.setNumPoints(size);
    data_.setNumPoints(size);
    for (int i = 0; i < size; ++i) source_data_[i] = 1.0f - samples[i];
    redraw();
}

void SimpleCurve::draw(applause::Canvas& canvas) {
    if (source_data_.numPoints() < 2 || width() <= 0.0f || height() <= 0.0f || canvas.totallyClamped()) return;

    const float line_width = std::max(0.0f, value(*this, kLineWidth));
    // Visage's graph coordinates include one native pixel of edge antialiasing.
    const float pixel = 1.0f / canvas.dpiScale();
    const float inset = std::min((line_width * 0.5f + pixel) / (height() + pixel), 0.5f);
    const float scale = 1.0f - 2.0f * inset;
    for (int i = 0; i < data_.numPoints(); ++i) data_[i] = inset + source_data_[i] * scale;

    const applause::Color fill_color = color(*this, kLine).withMultipliedAlpha(value(*this, kFillOpacity));
    canvas.setColor(applause::Brush::vertical(fill_color, fill_color.withAlpha(0.0f)));
    canvas.graphFill(data_, 0.0f, 0.0f, width(), height(), 1.0f);

    canvas.setColor(color(*this, kLine));
    canvas.graphLine(data_, 0.0f, 0.0f, width(), height(), line_width);
}

PlotView::PlotView() { setIgnoresMouseEvents(true, true); }

void PlotView::setGridDivisions(int columns, int rows) {
    columns_ = std::max(1, columns);
    rows_ = std::max(1, rows);
    redraw();
}

void PlotView::draw(applause::Canvas& canvas) {
    const float rounding = value(*this, kRounding);

    canvas.setColor(color(*this, kBackground));
    if (rounding > 0.0f)
        canvas.roundedRectangle(0.0f, 0.0f, width(), height(), rounding);
    else
        canvas.fill(0.0f, 0.0f, width(), height());

    canvas.setColor(color(*this, kBorder).withMultipliedAlpha(value(*this, kGridOpacity)));
    for (int column = 1; column < columns_; ++column) {
        const float x = width() * column / columns_;
        canvas.segment(x, 0.0f, x, height(), 1.0f, false);
    }
    for (int row = 1; row < rows_; ++row) {
        const float y = height() * row / rows_;
        canvas.segment(0.0f, y, width(), y, 1.0f, false);
    }

    const float border_width = value(*this, kBorderWidth);
    if (border_width > 0.0f) {
        canvas.setColor(color(*this, kBorder));
        if (rounding > 0.0f)
            canvas.roundedRectangleBorder(0.0f, 0.0f, width(), height(), rounding, border_width);
        else
            canvas.rectangleBorder(0.0f, 0.0f, width(), height(), border_width);
    }
}

}  // namespace applause
