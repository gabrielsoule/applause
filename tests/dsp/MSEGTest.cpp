#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <applause/dsp/modulation/MSEGCurve.h>
#include <applause/dsp/modulation/MSEGModulator.h>

#include <array>
#include <cmath>

using namespace applause;
using Catch::Approx;

static MSEGCurve<> makeRamp() {
    MSEGCurve<> c;
    c.points[0] = {0.0f, 0.0f};
    c.points[1] = {1.0f, 1.0f};
    c.num_points = 2;
    c.loop = true;
    return c;
}

static MSEGCurve<> makeTriangle() {
    MSEGCurve<> c;
    c.points[0] = {0.0f, 0.0f};
    c.points[1] = {0.5f, 1.0f};
    c.points[2] = {1.0f, 0.0f};
    c.num_points = 3;
    c.loop = true;
    return c;
}

TEST_CASE("MSEGCurve empty curve returns zero", "[dsp][mseg]")
{
    MSEGCurve<> c;
    REQUIRE(c.evaluate(0.0f) == 0.0f);
    REQUIRE(c.evaluate(0.5f) == 0.0f);
    REQUIRE(c.evaluate(1.0f) == 0.0f);

    SECTION("Single point also returns zero") {
        c.points[0] = {0.0f, 0.75f};
        c.num_points = 1;
        REQUIRE(c.evaluate(0.5f) == 0.0f);
    }
}

TEST_CASE("MSEGCurve two-point linear ramp", "[dsp][mseg]")
{
    auto c = makeRamp();

    SECTION("Output equals phase for a linear ramp") {
        REQUIRE(c.evaluate(0.0f) == Approx(0.0f));
        REQUIRE(c.evaluate(0.25f) == Approx(0.25f));
        REQUIRE(c.evaluate(0.5f) == Approx(0.5f));
        REQUIRE(c.evaluate(0.75f) == Approx(0.75f));
        REQUIRE(c.evaluate(1.0f) == Approx(1.0f));
    }

    SECTION("Inverted ramp") {
        c.points[0] = {0.0f, 1.0f};
        c.points[1] = {1.0f, 0.0f};
        REQUIRE(c.evaluate(0.0f) == Approx(1.0f));
        REQUIRE(c.evaluate(0.5f) == Approx(0.5f));
        REQUIRE(c.evaluate(1.0f) == Approx(0.0f));
    }
}

TEST_CASE("MSEGCurve boundary clamping", "[dsp][mseg]")
{
    auto c = makeRamp();

    SECTION("Phase below first point returns first Y") {
        REQUIRE(c.evaluate(-0.5f) == Approx(0.0f));
        REQUIRE(c.evaluate(-100.0f) == Approx(0.0f));
    }

    SECTION("Phase above last point returns last Y") {
        REQUIRE(c.evaluate(1.5f) == Approx(1.0f));
        REQUIRE(c.evaluate(100.0f) == Approx(1.0f));
    }
}

TEST_CASE("MSEGCurve multi-segment interpolation", "[dsp][mseg]")
{
    auto c = makeTriangle();

    SECTION("Endpoints are correct") {
        REQUIRE(c.evaluate(0.0f) == Approx(0.0f));
        REQUIRE(c.evaluate(1.0f) == Approx(0.0f));
    }

    SECTION("Peak at midpoint") {
        REQUIRE(c.evaluate(0.5f) == Approx(1.0f));
    }

    SECTION("Linear interpolation within segments") {
        REQUIRE(c.evaluate(0.25f) == Approx(0.5f));
        REQUIRE(c.evaluate(0.75f) == Approx(0.5f));
    }
}

TEST_CASE("MSEGCurve power curve", "[dsp][mseg]")
{
    auto c = makeRamp();

    SECTION("Positive power: midpoint < 0.5") {
        c.curvature_power[0] = 4.0f;
        float mid = c.evaluate(0.5f);
        REQUIRE(mid < 0.5f);
        REQUIRE(mid > 0.0f);
    }

    SECTION("Negative power: midpoint > 0.5") {
        c.curvature_power[0] = -4.0f;
        float mid = c.evaluate(0.5f);
        REQUIRE(mid > 0.5f);
        REQUIRE(mid < 1.0f);
    }

    SECTION("Endpoints unchanged regardless of power") {
        c.curvature_power[0] = 10.0f;
        REQUIRE(c.evaluate(0.0f) == Approx(0.0f));
        REQUIRE(c.evaluate(1.0f) == Approx(1.0f));
    }
}

TEST_CASE("MSEGModulator phase advancement", "[dsp][mseg]")
{
    auto curve = makeRamp();
    // 1 Hz at SR=100: each sample advances phase by 0.01.
    MSEGModulator mod(&curve, 100.0f);
    mod.setRate(1.0f);

    SECTION("Phase advances correctly") {
        mod.process(10);
        REQUIRE(mod.phase() == Approx(0.1f));

        mod.process(40);
        REQUIRE(mod.phase() == Approx(0.5f));
    }

    SECTION("Phase accumulates across multiple calls") {
        for (int i = 0; i < 10; i++)
            mod.process(10);
        REQUIRE(mod.phase() == Approx(0.0f).margin(1e-5));
    }
}

TEST_CASE("MSEGModulator loop wrapping", "[dsp][mseg]")
{
    auto curve = makeRamp();
    curve.loop = true;
    MSEGModulator mod(&curve, 100.0f);
    mod.setRate(1.0f);

    mod.process(110);
    REQUIRE(mod.phase() == Approx(0.1f).margin(1e-5));
}

TEST_CASE("MSEGModulator one-shot clamping", "[dsp][mseg]")
{
    auto curve = makeRamp();
    curve.loop = false;
    MSEGModulator mod(&curve, 100.0f);
    mod.setRate(1.0f);

    mod.process(150);
    REQUIRE(mod.phase() == Approx(1.0f));

    mod.process(50);
    REQUIRE(mod.phase() == Approx(1.0f));
}

TEST_CASE("MSEGModulator output matches curve", "[dsp][mseg]")
{
    auto curve = makeRamp();
    MSEGModulator mod(&curve, 100.0f);
    mod.setRate(1.0f);

    SECTION("Ramp identity: output equals phase") {
        mod.process(50);
        REQUIRE(mod.phase() == Approx(0.5f));
        REQUIRE(mod.value() == Approx(0.5f));
    }

    SECTION("Output matches curve.evaluate at each step") {
        for (int i = 0; i < 10; i++) {
            float result = mod.process(5);
            float expected = curve.evaluate(mod.phase());
            REQUIRE(result == Approx(expected));
        }
    }
}

TEST_CASE("MSEGModulator reset", "[dsp][mseg]")
{
    auto curve = makeRamp();
    MSEGModulator mod(&curve, 100.0f);
    mod.setRate(1.0f);

    mod.process(50);
    REQUIRE(mod.phase() == Approx(0.5f));

    mod.reset();
    REQUIRE(mod.phase() == Approx(0.0f));
    REQUIRE(mod.value() == Approx(0.0f));
}

TEST_CASE("MSEGModulator with triangle curve", "[dsp][mseg]")
{
    auto curve = makeTriangle();
    MSEGModulator mod(&curve, 100.0f);
    mod.setRate(1.0f);

    // At phase 0.25: triangle rising segment → 0.5
    mod.process(25);
    REQUIRE(mod.value() == Approx(0.5f));

    // At phase 0.50: triangle peak → 1.0
    mod.process(25);
    REQUIRE(mod.value() == Approx(1.0f));

    // At phase 0.75: triangle falling segment → 0.5
    mod.process(25);
    REQUIRE(mod.value() == Approx(0.5f));
}

