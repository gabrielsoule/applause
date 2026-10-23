#include <catch2/catch_test_macros.hpp>

#include <applause/ui/components/Button.h>
#include <applause/ui/components/RadioButton.h>
#include <applause/ui/components/RadioGroup.h>

#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void click(applause::Button& button) {
    applause::MouseEvent event;
    event.position = {1.0f, 1.0f};
    button.mouseDown(event);
    button.mouseUp(event);
}

}  // namespace

TEST_CASE("RadioGroup validates explicit membership and selects its first member", "[ui][radio-group]") {
    applause::RadioGroup group;
    applause::RadioGroup other;
    applause::Button first;
    applause::Button second;
    applause::Button third;
    REQUIRE(group.selectedId() == -1);
    REQUIRE_THROWS_AS(group.setSelectedId(0), std::out_of_range);
    REQUIRE_THROWS_AS(group.addButton(first, -1), std::invalid_argument);

    group.addButton(first, 8);
    second.setCheckable(true);
    second.setChecked(true);
    group.addButton(second, 2);
    REQUIRE(group.selectedId() == 8);
    REQUIRE(first.isChecked());
    REQUIRE_FALSE(second.isChecked());
    REQUIRE_THROWS_AS(group.addButton(third, 8), std::invalid_argument);
    REQUIRE_THROWS_AS(group.addButton(first, 4), std::invalid_argument);
    REQUIRE_THROWS_AS(other.addButton(second, 0), std::invalid_argument);
    REQUIRE_NOTHROW(group.removeButton(third));
}

TEST_CASE("RadioGroup selects across button types and parents before notifying observers", "[ui][radio-group]") {
    applause::RadioGroup group;
    applause::RadioButton first("First");
    applause::Button second("Second");
    applause::Frame left_parent;
    applause::Frame right_parent;
    left_parent.addChild(&first);
    right_parent.addChild(&second);
    second.setBounds(0.0f, 0.0f, 40.0f, 24.0f);
    group.addButton(first, 10);
    group.addButton(second, 20);
    REQUIRE(second.isCheckable());
    std::vector<int> changes;
    group.onSelectionChanged() += [&](int id) {
        REQUIRE(group.selectedId() == id);
        REQUIRE(first.isChecked() == (id == 10));
        REQUIRE(second.isChecked() == (id == 20));
        changes.push_back(id);
    };
    std::vector<bool> first_toggles;
    first.onToggle() += [&](bool on) {
        REQUIRE(second.isChecked() == !on);
        first_toggles.push_back(on);
    };
    int second_clicks = 0;
    second.onClick() += [&] { ++second_clicks; };

    click(second);
    REQUIRE(changes == std::vector<int>{20});
    REQUIRE(first_toggles == std::vector<bool>{false});
    // Clicking the selected member is still a click, but nothing changes.
    click(second);
    REQUIRE(second_clicks == 2);
    second.setChecked(false);
    second.setCheckedAndNotify(false);
    REQUIRE(group.selectedId() == 20);
    REQUIRE(second.isChecked());
    REQUIRE(changes == std::vector<int>{20});
    REQUIRE(first_toggles == std::vector<bool>{false});

    first.setChecked(true);
    REQUIRE(group.selectedId() == 10);
    REQUIRE(changes == std::vector<int>{20});
    REQUIRE(first_toggles == std::vector<bool>{false});
    second.setCheckedAndNotify(true);
    REQUIRE(changes == std::vector<int>{20, 20});
    REQUIRE(first_toggles == std::vector<bool>{false, false});
}

TEST_CASE("RadioGroup replaces members without disturbing selection or notifications", "[ui][radio-group]") {
    applause::RadioGroup group;
    applause::Button first;
    applause::Button second;
    applause::Button replacement;
    group.addButton(first, 0);
    group.addButton(second, 1);
    int notifications = 0;
    group.onSelectionChanged() += [&](int) { ++notifications; };

    group.replaceButton(0, replacement);
    REQUIRE(group.selectedId() == 0);
    REQUIRE(replacement.isChecked());
    REQUIRE_FALSE(second.isChecked());
    REQUIRE(notifications == 0);
    REQUIRE_THROWS_AS(group.replaceButton(5, first), std::out_of_range);
    REQUIRE_THROWS_AS(group.replaceButton(0, second), std::invalid_argument);
}

TEST_CASE("RadioGroup handles either member or group destruction first", "[ui][radio-group]") {
    auto group = std::make_unique<applause::RadioGroup>();
    auto first = std::make_unique<applause::Button>();
    applause::Button second;
    group->addButton(*first, 3);
    group->addButton(second, 9);
    int notifications = 0;
    group->onSelectionChanged() += [&](int) { ++notifications; };

    first.reset();
    REQUIRE(group->selectedId() == 9);
    REQUIRE(second.isChecked());
    REQUIRE(notifications == 0);
    group.reset();
    second.setChecked(false);
    REQUIRE_FALSE(second.isChecked());

    applause::RadioGroup empty;
    empty.addButton(second, 1);
    empty.removeButton(second);
    REQUIRE(empty.selectedId() == -1);
}
