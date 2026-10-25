#include <applause/graphics/Canvas.h>

#include <catch2/catch_test_macros.hpp>

using namespace applause;

namespace {
struct RegionFixture {
    RegionFixture() {
        canvas.setWindowless(200, 200);
        canvas.addRegion(&region);
        region.setBounds(0, 0, 200, 200);
        canvas.beginRegion(&region);
        canvas.setColor(0xffffffff);
    }

    Canvas canvas;
    Region region;
};
}  // namespace

TEST_CASE("ShapeBatcher groups disjoint shapes of one type into one batch", "[graphics]") {
    RegionFixture fixture;
    for (int i = 0; i < 100; ++i) fixture.canvas.rectangle((i % 10) * 20, (i / 10) * 20, 10, 10);
    fixture.canvas.endRegion();

    REQUIRE(fixture.region.numSubmitBatches() == 1);
}

TEST_CASE("ShapeBatcher keeps draw order across overlapping shape types", "[graphics]") {
    RegionFixture fixture;
    Canvas& canvas = fixture.canvas;
    Region& region = fixture.region;

    canvas.rectangle(0, 0, 50, 50);
    canvas.circle(10, 10, 20);

    SECTION("A shape over a later batch starts a new batch") {
        canvas.rectangle(20, 20, 10, 10);
        canvas.endRegion();

        REQUIRE(region.numSubmitBatches() == 3);
        REQUIRE(region.submitBatchAtPosition(0)->id() == region.submitBatchAtPosition(2)->id());
        REQUIRE(region.submitBatchAtPosition(0)->id() != region.submitBatchAtPosition(1)->id());
    }

    SECTION("A shape clear of later batches joins the earlier matching batch") {
        canvas.rectangle(150, 150, 10, 10);
        canvas.endRegion();

        REQUIRE(region.numSubmitBatches() == 2);
    }

    SECTION("A shape matching the newest batch joins it") {
        canvas.rectangle(20, 20, 10, 10);
        canvas.rectangle(150, 150, 10, 10);
        canvas.endRegion();

        REQUIRE(region.numSubmitBatches() == 3);
    }
}
