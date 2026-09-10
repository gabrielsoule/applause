#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <applause/core/PluginBase.h>
#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/components/SelectionGrid.h>
#include <applause/ui/components/ParamSelectionGrid.h>
#include <applause/util/ParamMessageQueue.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using Catch::Approx;

namespace {

const clap_plugin_descriptor_t kDescriptor{};

struct TestPlugin : applause::PluginBase {
    applause::ParamsExtension params{8};

    TestPlugin() : PluginBase(&kDescriptor, nullptr) { registerExtension(params); }
    applause::ProcessStatus process(applause::ProcessContext&) noexcept override {
        return applause::ProcessStatus::Continue;
    }
};

applause::ParamConfig choiceParam(std::string id, float min, float max, float value, bool stepped = true) {
    return applause::ParamConfig{
        .string_id = std::move(id),
        .min_value = min,
        .max_value = max,
        .default_value = value,
        .is_stepped = stepped,
        .value_to_text = [](float option, const applause::ParamInfo&) {
            return "Option " + std::to_string(static_cast<int>(option));
        },
    };
}

struct CustomCell final : applause::SelectionGridCell {
    explicit CustomCell(int value) : value(value) {}
    int value = 0;
};

void click(applause::SelectionGridCell& cell, bool release_inside = true) {
    applause::MouseEvent down;
    down.position = {1.0f, 1.0f};
    down.is_down = true;
    cell.mouseDown(down);

    applause::MouseEvent up;
    up.position = release_inside ? applause::Point{1.0f, 1.0f} : applause::Point{-1.0f, -1.0f};
    cell.mouseUp(up);
}

}  // namespace

TEST_CASE("SelectionGrid validates dimensions and labels", "[ui][selection-grid]") {
    REQUIRE_THROWS_AS(applause::SelectionGrid(0, 1), std::invalid_argument);
    REQUIRE_THROWS_AS(applause::SelectionGrid(1, -1), std::invalid_argument);
    REQUIRE_THROWS_AS(applause::SelectionGrid(std::numeric_limits<int>::max(), 2), std::invalid_argument);
    REQUIRE_THROWS_AS(applause::SelectionGrid(2, 2, {"A", "B", "C"}), std::invalid_argument);

    applause::SelectionGrid grid(2, 2, {"A", "B", "C", "D"});
    REQUIRE(grid.selectedIndex() == 0);
    REQUIRE(grid.cell(0).selected());
    REQUIRE_FALSE(grid.cell(1).selected());
    REQUIRE_THROWS_AS(grid.cell(-1), std::out_of_range);
    REQUIRE_THROWS_AS(grid.cell(4), std::out_of_range);

    REQUIRE(dynamic_cast<const applause::TextSelectionGridCell&>(grid.cell(0)).text().toUtf8() == "A");
    REQUIRE(dynamic_cast<const applause::TextSelectionGridCell&>(grid.cell(3)).text().toUtf8() == "D");
}

// Every section pins the gap and padding on a palette so the layout math is checked independently
// of whatever the theme defaults happen to be.
TEST_CASE("SelectionGrid lays cells out in row-major order with gaps", "[ui][selection-grid]") {
    applause::Palette palette;
    palette.initWithDefaults();
    palette.setValue(applause::SelectionGrid::ApplauseSelectionGridGap, 5.0f);
    palette.setValue(applause::SelectionGrid::ApplauseSelectionGridPadding, 5.0f);

    SECTION("two-dimensional grid") {
        applause::SelectionGrid grid(2, 2);
        grid.setPalette(&palette);
        grid.setBounds(0.0f, 0.0f, 105.0f, 55.0f);

        const auto& first = grid.cell(0).bounds();
        const auto& second = grid.cell(1).bounds();
        const auto& third = grid.cell(2).bounds();
        const auto& fourth = grid.cell(3).bounds();

        REQUIRE(first.x() == Approx(5.0f));
        REQUIRE(first.y() == Approx(5.0f));
        REQUIRE(first.width() == Approx(45.0f));
        REQUIRE(first.height() == Approx(20.0f));
        REQUIRE(second.x() == Approx(55.0f));
        REQUIRE(second.y() == Approx(5.0f));
        REQUIRE(third.x() == Approx(5.0f));
        REQUIRE(third.y() == Approx(30.0f));
        REQUIRE(fourth.right() == Approx(100.0f));
        REQUIRE(fourth.bottom() == Approx(50.0f));
    }

    SECTION("single row") {
        applause::SelectionGrid grid(3, 1);
        grid.setPalette(&palette);
        grid.setBounds(0.0f, 0.0f, 110.0f, 20.0f);

        REQUIRE(grid.cell(0).x() == Approx(5.0f));
        REQUIRE(grid.cell(0).width() == Approx(30.0f));
        REQUIRE(grid.cell(1).x() == Approx(40.0f));
        REQUIRE(grid.cell(2).right() == Approx(105.0f));
        REQUIRE(grid.cell(2).bottom() == Approx(15.0f));
    }

    SECTION("single column") {
        applause::SelectionGrid grid(1, 3);
        grid.setPalette(&palette);
        grid.setBounds(0.0f, 0.0f, 20.0f, 110.0f);

        REQUIRE(grid.cell(0).y() == Approx(5.0f));
        REQUIRE(grid.cell(0).height() == Approx(30.0f));
        REQUIRE(grid.cell(1).y() == Approx(40.0f));
        REQUIRE(grid.cell(2).right() == Approx(15.0f));
        REQUIRE(grid.cell(2).bottom() == Approx(105.0f));
    }

    SECTION("theme controls the gap and outer padding") {
        palette.setValue(applause::SelectionGrid::ApplauseSelectionGridPadding, 7.0f);

        applause::SelectionGrid grid(2, 1);
        grid.setPalette(&palette);
        grid.setBounds(0.0f, 0.0f, 105.0f, 30.0f);

        REQUIRE(grid.cell(0).x() == Approx(7.0f));
        REQUIRE(grid.cell(0).y() == Approx(7.0f));
        REQUIRE(grid.cell(0).width() == Approx(43.0f));
        REQUIRE(grid.cell(0).height() == Approx(16.0f));
        REQUIRE(grid.cell(1).x() == Approx(55.0f));
        REQUIRE(grid.cell(1).right() == Approx(98.0f));
        REQUIRE(grid.cell(1).bottom() == Approx(23.0f));
    }

    SECTION("oversized padding never creates negative cell bounds") {
        palette.setValue(applause::SelectionGrid::ApplauseSelectionGridPadding, 100.0f);

        applause::SelectionGrid grid(2, 2);
        grid.setPalette(&palette);
        grid.setBounds(0.0f, 0.0f, 7.0f, 5.0f);

        for (int index = 0; index < 4; ++index) {
            REQUIRE(grid.cell(index).width() == Approx(0.0f));
            REQUIRE(grid.cell(index).height() == Approx(0.0f));
        }
    }
}

TEST_CASE("SelectionGrid owns replaceable custom cells", "[ui][selection-grid]") {
    applause::SelectionGrid grid(3, 1);
    auto& custom = grid.emplaceCell<CustomCell>(1, 42);

    REQUIRE(&grid.cell(1) == &custom);
    REQUIRE(custom.index() == 1);
    REQUIRE(custom.value == 42);
    REQUIRE_FALSE(custom.selected());

    grid.init();
    REQUIRE_THROWS_AS(grid.emplaceCell<CustomCell>(2, 7), std::logic_error);
}

TEST_CASE("SelectionGrid distinguishes silent and notifying selection", "[ui][selection-grid]") {
    applause::SelectionGrid grid(2, 2);
    std::vector<int> changes;
    grid.onSelectionChanged() += [&](int index) { changes.push_back(index); };

    grid.setSelectedIndex(2);
    REQUIRE(grid.selectedIndex() == 2);
    REQUIRE(grid.cell(2).selected());
    REQUIRE_FALSE(grid.cell(0).selected());
    REQUIRE(changes.empty());

    grid.setBounds(0.0f, 0.0f, 100.0f, 80.0f);
    grid.setBounds(0.0f, 0.0f, 80.0f, 100.0f);
    REQUIRE(grid.selectedIndex() == 2);
    REQUIRE(grid.cell(2).selected());

    grid.setSelectedIndexAndNotify(3);
    grid.setSelectedIndexAndNotify(3);
    REQUIRE(changes == std::vector<int>{3});

    REQUIRE_THROWS_AS(grid.setSelectedIndex(-1), std::out_of_range);
    REQUIRE_THROWS_AS(grid.setSelectedIndexAndNotify(4), std::out_of_range);
}

TEST_CASE("SelectionGrid cells use child-frame mouse activation", "[ui][selection-grid]") {
    applause::SelectionGrid grid(2, 2);
    grid.setBounds(0.0f, 0.0f, 100.0f, 100.0f);
    std::vector<int> changes;
    grid.onSelectionChanged() += [&](int index) { changes.push_back(index); };

    REQUIRE(grid.frameAtPoint({25.0f, 25.0f}) == &grid.cell(0));
    REQUIRE(grid.frameAtPoint({75.0f, 25.0f}) == &grid.cell(1));
    REQUIRE(grid.frameAtPoint({1.0f, 1.0f}) == &grid);
    REQUIRE(grid.frameAtPoint({50.0f, 25.0f}) == &grid);
    REQUIRE(grid.frameAtPoint({50.0f, 50.0f}) == &grid);

    grid.cell(3).onToggle().clear();
    click(grid.cell(3));
    REQUIRE(grid.selectedIndex() == 3);
    REQUIRE(changes == std::vector<int>{3});

    click(grid.cell(2), false);
    REQUIRE(grid.selectedIndex() == 3);
    REQUIRE(changes == std::vector<int>{3});

    click(grid.cell(3));
    REQUIRE(changes == std::vector<int>{3});
}

TEST_CASE("ParamSelectionGrid validates parameter semantics", "[ui][selection-grid][params]") {
    SECTION("parameter must be stepped") {
        TestPlugin plugin;
        plugin.params.registerParam(choiceParam("mode", 0.0f, 2.0f, 0.0f, false));
        REQUIRE_THROWS_AS(applause::ParamSelectionGrid(plugin.params.getInfo("mode"), 3, 1), std::invalid_argument);
    }

    SECTION("parameter bounds must be integral") {
        TestPlugin plugin;
        plugin.params.registerParam(choiceParam("mode", -1.5f, 1.5f, 0.0f));
        REQUIRE_THROWS_AS(applause::ParamSelectionGrid(plugin.params.getInfo("mode"), 3, 1), std::invalid_argument);
    }

    SECTION("cell count must match the inclusive parameter range") {
        TestPlugin plugin;
        plugin.params.registerParam(choiceParam("mode", 0.0f, 3.0f, 0.0f));
        REQUIRE_THROWS_AS(applause::ParamSelectionGrid(plugin.params.getInfo("mode"), 3, 1), std::invalid_argument);
    }
}

TEST_CASE("ParamSelectionGrid derives labels and maps parameter minima", "[ui][selection-grid][params]") {
    SECTION("negative minimum") {
        TestPlugin plugin;
        plugin.params.registerParam(choiceParam("mode", -1.0f, 2.0f, 0.0f));

        applause::ParamSelectionGrid grid(plugin.params.getInfo("mode"), 2, 2);
        REQUIRE(grid.selectedIndex() == 1);
        REQUIRE(dynamic_cast<const applause::TextSelectionGridCell&>(grid.cell(0)).text().toUtf8() == "Option -1");
        REQUIRE(dynamic_cast<const applause::TextSelectionGridCell&>(grid.cell(3)).text().toUtf8() == "Option 2");

        auto& custom = grid.emplaceCell<CustomCell>(2, 99);
        REQUIRE(custom.index() == 2);
        REQUIRE(custom.value == 99);
    }

    SECTION("zero minimum") {
        TestPlugin plugin;
        plugin.params.registerParam(choiceParam("mode", 0.0f, 1.0f, 1.0f));

        applause::ParamSelectionGrid grid(plugin.params.getInfo("mode"), 2, 1);
        REQUIRE(grid.selectedIndex() == 1);
        REQUIRE(dynamic_cast<const applause::TextSelectionGridCell&>(grid.cell(0)).text().toUtf8() == "Option 0");
        REQUIRE(dynamic_cast<const applause::TextSelectionGridCell&>(grid.cell(1)).text().toUtf8() == "Option 1");
    }

    SECTION("positive nonzero minimum") {
        TestPlugin plugin;
        plugin.params.registerParam(choiceParam("mode", 4.0f, 6.0f, 5.0f));

        applause::ParamSelectionGrid grid(plugin.params.getInfo("mode"), 1, 3);
        REQUIRE(grid.selectedIndex() == 1);
        REQUIRE(dynamic_cast<const applause::TextSelectionGridCell&>(grid.cell(0)).text().toUtf8() == "Option 4");
        REQUIRE(dynamic_cast<const applause::TextSelectionGridCell&>(grid.cell(2)).text().toUtf8() == "Option 6");
    }
}

TEST_CASE("ParamSelectionGrid sends one complete gesture and follows parameter changes silently",
          "[ui][selection-grid][params]") {
    TestPlugin plugin;
    plugin.params.registerParam(choiceParam("mode", -1.0f, 2.0f, -1.0f));
    auto& parameter = plugin.params.getInfo("mode");
    applause::ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);

    applause::ParamSelectionGrid grid(parameter, 2, 2);
    grid.setBounds(0.0f, 0.0f, 100.0f, 100.0f);
    std::vector<int> changes;
    grid.onSelectionChanged() = [&](int index) { changes.push_back(index); };

    click(grid.cell(2));
    REQUIRE(grid.selectedIndex() == 2);
    REQUIRE(parameter.getValue() == 1.0f);
    REQUIRE(changes == std::vector<int>{2});

    applause::ParamMessageQueue::Message message{};
    REQUIRE(queue.toAudio().try_dequeue(message));
    REQUIRE(message.type == applause::ParamMessageQueue::BEGIN_GESTURE);
    REQUIRE(message.paramId == parameter.clapId);
    REQUIRE(queue.toAudio().try_dequeue(message));
    REQUIRE(message.type == applause::ParamMessageQueue::PARAM_VALUE);
    REQUIRE(message.paramId == parameter.clapId);
    REQUIRE(message.value == 1.0f);
    REQUIRE(queue.toAudio().try_dequeue(message));
    REQUIRE(message.type == applause::ParamMessageQueue::END_GESTURE);
    REQUIRE(message.paramId == parameter.clapId);
    REQUIRE_FALSE(queue.toAudio().try_dequeue(message));

    parameter.setValueSilently(2.0f);
    parameter.on_value_changed(2.0f);
    REQUIRE(grid.selectedIndex() == 3);
    REQUIRE(changes == std::vector<int>{2});

    grid.onSelectionChanged().clear();
    click(grid.cell(1));
    REQUIRE(grid.selectedIndex() == 1);
    REQUIRE(parameter.getValue() == 0.0f);
    REQUIRE(changes == std::vector<int>{2});
}
