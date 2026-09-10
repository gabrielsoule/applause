#include <catch2/catch_test_macros.hpp>

#include <applause/ui/components/Plot.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <span>

TEST_CASE("PlotView provides a background, grid, and border for composed content", "[ui][plot]") {
    applause::PlotView plot;
    applause::SimpleCurve trace;

    REQUIRE(plot.children().empty());

    plot.setBounds(12.0f, 8.0f, 160.0f, 80.0f);
    plot.addChild(&trace);
    trace.setBounds(plot.localBounds());

    REQUIRE(plot.children().size() == 1);
    REQUIRE(plot.children().front() == &trace);
    REQUIRE(trace.parent() == &plot);
    REQUIRE(trace.bounds() == plot.localBounds());
    REQUIRE(plot.ignoresMouseEvents());
    REQUIRE(trace.ignoresMouseEvents());
    REQUIRE(plot.frameAtPoint({80.0f, 40.0f}) == nullptr);

    applause::Frame interactive;
    plot.addChild(&interactive);
    interactive.setBounds(plot.localBounds());
    REQUIRE(plot.frameAtPoint({80.0f, 40.0f}) == &interactive);

    REQUIRE_NOTHROW(plot.setGridDivisions(3, 2));
    REQUIRE_NOTHROW(plot.setGridDivisions(0, -1));
}

TEST_CASE("PlotView exposes themed rounding and border width", "[ui][plot]") {
    applause::PlotView plot;

    REQUIRE(plot.paletteValue(applause::PlotView::ApplausePlotRounding) == 4.0f);
    REQUIRE(plot.paletteValue(applause::PlotView::ApplausePlotBorderWidth) == 1.0f);

    applause::Palette palette;
    palette.initWithDefaults();
    palette.setValue(applause::PlotView::ApplausePlotRounding, 9.0f);
    palette.setValue(applause::PlotView::ApplausePlotBorderWidth, 3.0f);
    plot.setPalette(&palette);

    REQUIRE(plot.paletteValue(applause::PlotView::ApplausePlotRounding) == 9.0f);
    REQUIRE(plot.paletteValue(applause::PlotView::ApplausePlotBorderWidth) == 3.0f);
}

TEST_CASE("SimpleCurve uses the MSEG fill color", "[ui][plot]") {
    applause::SimpleCurve trace;

    REQUIRE(trace.paletteColor(applause::SimpleCurve::ApplauseTraceFill).gradient().sample(0.0f) ==
            applause::Color(0x309966ff));
}

TEST_CASE("SimpleCurve draws fewer than two samples as no-ops", "[ui][plot]") {
    applause::Canvas canvas;
    applause::SimpleCurve trace;

    trace.setSamples(std::span<const float>{});
    REQUIRE_NOTHROW(trace.draw(canvas));

    const std::array sample{0.5f};
    trace.setSamples(sample);
    REQUIRE_NOTHROW(trace.draw(canvas));

    const std::array samples{0.0f, 1.0f};
    trace.setSamples(samples);
    trace.setBounds(0.0f, 0.0f, 0.0f, 0.0f);
    REQUIRE_NOTHROW(trace.draw(canvas));
}

TEST_CASE("SimpleCurve keeps its stroke inside the normalized limits", "[ui][plot]") {
    if (!std::getenv("APPLAUSE_RUN_WINDOWLESS_GPU_TESTS"))
        SKIP("Set APPLAUSE_RUN_WINDOWLESS_GPU_TESTS to run the GPU pixel probe");

    constexpr int width = 192;
    constexpr int height = 64;

    applause::Palette palette;
    palette.initWithDefaults();
    palette.setColor(applause::SimpleCurve::ApplauseTraceLine, applause::Color(0xffffffff));
    palette.setColor(applause::SimpleCurve::ApplauseTraceFill, applause::Color(0x00000000));
    palette.setValue(applause::SimpleCurve::ApplauseTraceLineWidth, 4.0f);
    palette.setColor(applause::PlotView::ApplausePlotBackground, applause::Color(0xff000000));
    palette.setColor(applause::PlotView::ApplausePlotGrid, applause::Color(0x00000000));
    palette.setColor(applause::PlotView::ApplausePlotBorder, applause::Color(0x00000000));
    palette.setValue(applause::PlotView::ApplausePlotRounding, 0.0f);
    palette.setValue(applause::PlotView::ApplausePlotBorderWidth, 0.0f);

    std::array<float, width> samples;
    for (int i = 0; i < width; ++i)
        samples[i] = i < width / 3 ? 1.0f : (i < 2 * width / 3 ? 0.5f : 0.0f);

    applause::Canvas canvas;
    applause::PlotView plot;
    applause::SimpleCurve trace;
    trace.setSamples(samples);
    canvas.setPalette(&palette);
    canvas.setWindowless(width, height);
    plot.setBounds(0.0f, 0.0f, width, height);
    trace.setBounds(plot.localBounds());
    plot.setGridDivisions(1, 1);

    plot.draw(canvas);
    trace.draw(canvas);
    canvas.submit();
    const auto screenshot = canvas.takeScreenshot();

    const auto brightness = [&](int center) {
        int total = 0;
        for (int x = center - 12; x < center + 12; ++x) {
            for (int y = 0; y < height; ++y) {
                const auto pixel = screenshot.sample(x, y);
                total += pixel.hexRed() + pixel.hexGreen() + pixel.hexBlue();
            }
        }
        return total;
    };

    const int top = brightness(width / 6);
    const int middle = brightness(width / 2);
    const int bottom = brightness(5 * width / 6);

    REQUIRE(middle > 0);
    REQUIRE(top * 5 > middle * 4);
    REQUIRE(top * 4 < middle * 5);
    REQUIRE(bottom * 5 > middle * 4);
    REQUIRE(bottom * 4 < middle * 5);
}

TEST_CASE("SimpleCurve copies samples and maps low values toward the bottom", "[ui][plot]") {
    if (!std::getenv("APPLAUSE_RUN_WINDOWLESS_GPU_TESTS"))
        SKIP("Set APPLAUSE_RUN_WINDOWLESS_GPU_TESTS to run the GPU pixel probe");

    constexpr int size = 64;

    applause::Palette palette;
    palette.initWithDefaults();
    palette.setColor(applause::SimpleCurve::ApplauseTraceLine, applause::Color(0xffffffff));
    palette.setColor(applause::SimpleCurve::ApplauseTraceFill, applause::Color(0xff0000ff));
    palette.setValue(applause::SimpleCurve::ApplauseTraceLineWidth, 2.0f);
    palette.setColor(applause::PlotView::ApplausePlotBackground, applause::Color(0xff000000));
    palette.setColor(applause::PlotView::ApplausePlotGrid, applause::Color(0xffff0000));
    palette.setColor(applause::PlotView::ApplausePlotBorder, applause::Color(0xff000000));

    applause::Canvas canvas;
    applause::PlotView plot;
    applause::SimpleCurve trace;
    canvas.setPalette(&palette);
    canvas.setWindowless(size, size);
    plot.setBounds(0.0f, 0.0f, size, size);
    trace.setBounds(plot.localBounds());
    plot.setGridDivisions(3, 2);

    std::array samples{0.25f, 0.25f};
    trace.setSamples(samples);
    samples.fill(0.75f);

    plot.draw(canvas);
    trace.draw(canvas);
    canvas.submit();
    const auto screenshot = canvas.takeScreenshot();

    int lower_red = 0;
    int upper_red = 0;
    for (int y = 10; y <= 54; ++y) {
        const int red = screenshot.sample(size / 2, y).hexRed();
        if (y <= 24) upper_red = std::max(upper_red, red);
        if (y >= 42) lower_red = std::max(lower_red, red);
    }

    int fill_blue = 0;
    int empty_blue = 0;
    for (int y = 20; y <= 56; ++y) {
        const int blue = screenshot.sample(size / 2, y).hexBlue();
        if (y <= 36) empty_blue = std::max(empty_blue, blue);
        if (y >= 51) fill_blue = std::max(fill_blue, blue);
    }

    int grid_red = 0;
    for (int x = 18; x <= 24; ++x)
        grid_red = std::max(grid_red, static_cast<int>(screenshot.sample(x, 10).hexRed()));

    REQUIRE(lower_red > 200);
    REQUIRE(upper_red < 32);
    REQUIRE(fill_blue > 24);
    REQUIRE(empty_blue < 16);
    REQUIRE(grid_red > 128);
}
