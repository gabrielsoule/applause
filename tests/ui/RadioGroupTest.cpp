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
    applause::ToggleButton first;
    applause::ToggleButton second;
    applause::ToggleButton third;
    REQUIRE(group.selectedId() == -1);
    REQUIRE_THROWS_AS(group.setSelectedId(0), std::out_of_range);
    REQUIRE_THROWS_AS(group.addButton(first, -1), std::invalid_argument);

    group.addButton(first, 8);
    second.setToggled(true);
    group.addButton(second, 2);
    REQUIRE(group.selectedId() == 8);
    REQUIRE(first.toggled());
    REQUIRE_FALSE(second.toggled());
    REQUIRE_THROWS_AS(group.addButton(third, 8), std::invalid_argument);
    REQUIRE_THROWS_AS(group.addButton(first, 4), std::invalid_argument);
    REQUIRE_THROWS_AS(other.addButton(second, 0), std::invalid_argument);
    REQUIRE_NOTHROW(group.removeButton(third));
}

TEST_CASE("RadioGroup selects across button types and parents before notifying observers", "[ui][radio-group]") {
    applause::RadioGroup group;
    applause::RadioButton first("First");
    applause::ToggleTextButton second("Second");
    applause::Frame left_parent;
    applause::Frame right_parent;
    left_parent.addChild(&first);
    right_parent.addChild(&second);
    second.setBounds(0.0f, 0.0f, 40.0f, 24.0f);
    group.addButton(first, 10);
    group.addButton(second, 20);
    std::vector<int> changes;
    group.onSelectionChanged() += [&](int id) {
        REQUIRE(group.selectedId() == id);
        REQUIRE(first.toggled() == (id == 10));
        REQUIRE(second.toggled() == (id == 20));
        changes.push_back(id);
    };
    int first_activations = 0;
    first.onToggle() += [&](applause::Button*, bool) { ++first_activations; };

    second.onToggle().clear();
    click(second);
    REQUIRE(changes == std::vector<int>{20});
    REQUIRE(first_activations == 0);
    click(second);
    second.setToggled(false);
    second.setToggledAndNotify(false);
    REQUIRE(group.selectedId() == 20);
    REQUIRE(second.toggled());
    REQUIRE(changes == std::vector<int>{20});

    first.setToggled(true);
    REQUIRE(group.selectedId() == 10);
    REQUIRE(changes == std::vector<int>{20});
    second.setToggledAndNotify(true);
    REQUIRE(changes == std::vector<int>{20, 20});
}

TEST_CASE("RadioGroup replaces members without disturbing selection or notifications", "[ui][radio-group]") {
    applause::RadioGroup group;
    applause::ToggleButton first;
    applause::ToggleButton second;
    applause::ToggleButton replacement;
    group.addButton(first, 0);
    group.addButton(second, 1);
    int notifications = 0;
    group.onSelectionChanged() += [&](int) { ++notifications; };

    group.replaceButton(0, replacement);
    REQUIRE(group.selectedId() == 0);
    REQUIRE(replacement.toggled());
    REQUIRE_FALSE(second.toggled());
    REQUIRE(notifications == 0);
    REQUIRE_THROWS_AS(group.replaceButton(5, first), std::out_of_range);
    REQUIRE_THROWS_AS(group.replaceButton(0, second), std::invalid_argument);
}

TEST_CASE("RadioGroup handles either member or group destruction first", "[ui][radio-group]") {
    auto group = std::make_unique<applause::RadioGroup>();
    auto first = std::make_unique<applause::ToggleButton>();
    applause::ToggleButton second;
    group->addButton(*first, 3);
    group->addButton(second, 9);
    int notifications = 0;
    group->onSelectionChanged() += [&](int) { ++notifications; };

    first.reset();
    REQUIRE(group->selectedId() == 9);
    REQUIRE(second.toggled());
    REQUIRE(notifications == 0);
    group.reset();
    second.setToggled(false);
    REQUIRE_FALSE(second.toggled());

    applause::RadioGroup empty;
    empty.addButton(second, 1);
    empty.removeButton(second);
    REQUIRE(empty.selectedId() == -1);
}
