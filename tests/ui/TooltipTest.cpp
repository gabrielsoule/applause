#include <catch2/catch_test_macros.hpp>

#include <applause/ui/ApplauseEditor.h>
#include <applause/ui/Tooltip.h>

#include <optional>

TEST_CASE("Tooltips attach to a new frame at a previously used address", "[ui][tooltip]") {
    applause::ApplauseEditor editor;
    editor.setBounds(0.0f, 0.0f, 400.0f, 300.0f);
    auto& display = editor.tooltipDisplay();
    std::optional<applause::Frame> frame{std::in_place};
    const auto* address = &*frame;

    editor.addChild(*frame);
    applause::setTooltip(*frame, "First frame");
    frame->processMouseEnter({});
    REQUIRE(display.isRunning());
    display.hide();
    REQUIRE_FALSE(display.isRunning());

    SECTION("destroying an enabled binding") {}
    SECTION("destroying a disabled binding") {
        applause::removeTooltip(*frame);
    }

    frame.reset();
    frame.emplace();
    REQUIRE(&*frame == address);
    editor.addChild(*frame);
    applause::setTooltip(*frame, "Replacement frame");
    frame->processMouseEnter({});
    REQUIRE(display.isRunning());
    frame->processMouseExit({});
    REQUIRE_FALSE(display.isRunning());
}
