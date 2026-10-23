#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <applause/ui/components/Button.h>

#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kIcon =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path d="M2 2 L22 2 L22 22 L2 22 Z"/></svg>)";

applause::Svg icon() { return {reinterpret_cast<const unsigned char*>(kIcon.data()), static_cast<int>(kIcon.size())}; }

void click(applause::Button& button, bool release_inside = true) {
    applause::MouseEvent down;
    down.position = {1.0f, 1.0f};
    down.is_down = true;
    button.mouseDown(down);

    applause::MouseEvent up;
    up.position = release_inside ? applause::Point{1.0f, 1.0f} : applause::Point{-1.0f, -1.0f};
    button.mouseUp(up);
}

}  // namespace

TEST_CASE("A plain button runs onClick on each click and never onToggle", "[ui][button]") {
    applause::Button button("OK");
    button.setBounds(0.0f, 0.0f, 80.0f, 24.0f);
    int clicks = 0;
    int toggles = 0;
    button.onClick() += [&] { ++clicks; };
    button.onToggle() += [&](bool) { ++toggles; };

    click(button);
    click(button);
    REQUIRE(clicks == 2);
    REQUIRE(toggles == 0);
    REQUIRE_FALSE(button.isChecked());
}

TEST_CASE("A checkable button turns on or off before onClick runs", "[ui][button]") {
    applause::Button button("Toggle");
    button.setCheckable(true);
    button.setBounds(0.0f, 0.0f, 80.0f, 24.0f);
    std::vector<std::string> events;
    button.onToggle() += [&](bool on) { events.push_back(on ? "on" : "off"); };
    button.onClick() += [&] { events.push_back(button.isChecked() ? "click while on" : "click while off"); };

    click(button);
    click(button);
    REQUIRE(events == std::vector<std::string>{"on", "click while on", "off", "click while off"});
}

TEST_CASE("Only notifying changes of the checked state run onToggle", "[ui][button]") {
    applause::Button button;
    button.setCheckable(true);
    std::vector<bool> toggles;
    int clicks = 0;
    button.onToggle() += [&](bool on) { toggles.push_back(on); };
    button.onClick() += [&] { ++clicks; };

    button.setChecked(true);
    button.setCheckedAndNotify(true);
    REQUIRE(button.isChecked());
    REQUIRE(toggles.empty());

    button.setCheckedAndNotify(false);
    REQUIRE(toggles == std::vector<bool>{false});
    REQUIRE(clicks == 0);

    button.click();
    REQUIRE(button.isChecked());
    REQUIRE(toggles == std::vector<bool>{false, true});
    REQUIRE(clicks == 1);
}

TEST_CASE("A button clicks only while enabled and when released over it", "[ui][button]") {
    applause::Button button("OK");
    button.setBounds(0.0f, 0.0f, 80.0f, 24.0f);
    int clicks = 0;
    button.onClick() += [&] { ++clicks; };

    button.setEnabled(false);
    click(button);
    REQUIRE(clicks == 0);
    button.setEnabled(true);
    click(button, false);
    REQUIRE(clicks == 0);

    button.setTriggeredOnMouseDown(true);
    click(button, false);
    REQUIRE(clicks == 1);
}

TEST_CASE("A disabled button's hover stays at zero", "[ui][button]") {
    struct CustomButton : applause::Button {
        using Button::updateHover;
    } button;
    button.setEnabled(false);
    button.mouseEnter({});
    REQUIRE(button.updateHover() == 0.0f);
}

TEST_CASE("Turning checking off leaves a button unchecked", "[ui][button]") {
    applause::Button button("Toggle");
    button.setCheckable(true);
    button.setChecked(true);
    button.setCheckable(false);
    REQUIRE_FALSE(button.isChecked());

    button.click();
    REQUIRE_FALSE(button.isChecked());
}

TEST_CASE("A button's preferred width fits its padding, text, and icon", "[ui][button]") {
    applause::Button empty;
    applause::Button text("Some text");
    applause::Button icon_only(icon());
    applause::Button icon_and_text(icon(), "Some text");

    REQUIRE(empty.preferredWidth() == Catch::Approx(20.0f));
    REQUIRE(text.preferredWidth() > empty.preferredWidth());
    REQUIRE(icon_only.preferredWidth() == Catch::Approx(20.0f + 16.0f));
    REQUIRE(icon_and_text.preferredWidth() == Catch::Approx(text.preferredWidth() + 16.0f + 6.0f));
}

TEST_CASE("A button's icon is centered with its text and follows the icon size token", "[ui][button]") {
    applause::Button icon_only(icon());
    icon_only.setBounds(0.0f, 0.0f, 200.0f, 30.0f);
    const applause::Frame& only = *icon_only.children().front();
    REQUIRE(only.x() == Catch::Approx(92.0f));
    REQUIRE(only.y() == Catch::Approx(7.0f));
    REQUIRE(only.width() == Catch::Approx(16.0f));
    REQUIRE(icon_only.frameAtPoint({100.0f, 15.0f}) == &icon_only);

    icon_only.set(applause::Button::kIconSize, 20.0f);
    REQUIRE(only.width() == Catch::Approx(20.0f));
    REQUIRE(only.x() == Catch::Approx(90.0f));

    applause::Button icon_and_text(icon(), "Load");
    icon_and_text.setBounds(0.0f, 0.0f, 200.0f, 30.0f);
    const applause::Frame& beside = *icon_and_text.children().front();
    const float content_width = icon_and_text.preferredWidth() - 20.0f;
    REQUIRE(beside.x() == Catch::Approx((200.0f - content_width) * 0.5f));
}
