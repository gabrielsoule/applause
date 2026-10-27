#include <catch2/catch_test_macros.hpp>

#include <applause/ui/ApplicationEditor.h>
#include <applause/ui/components/TextEditor.h>
#include <applause/util/ThreadUtils.h>
#include <applause/windowing/Windowing.h>

#include <memory>
#include <thread>

namespace {

// An editor in an unshown native window, so clicks take the platform's path through the window's event handler, which
// decides keyboard focus. Creating a window marks this thread as the main thread; that is put back afterwards, since
// tests share one process in a random order.
struct ClickableWindow {
    ClickableWindow() {
        // Every click counts as a single click.
        applause::Window::setDoubleClickSpeed(0);
        editor.addToWindow(window.get());
    }

    ~ClickableWindow() {
        editor.removeFromWindow();
        applause::Window::setDoubleClickSpeed(doubleClickSpeed);
        applause::Thread::main_thread_set_ = mainThreadSet;
        applause::Thread::main_thread_id_ = mainThreadId;
    }

    void click(const applause::Frame& frame, float x) {
        const auto point = window->convertToNative(frame.positionInWindow() + applause::Point(x, frame.height() / 2));
        window->handleMouseDown(applause::kMouseButtonLeft, point.x, point.y, applause::kMouseButtonLeft, 0);
        window->handleMouseUp(applause::kMouseButtonLeft, point.x, point.y, 0, 0);
    }

    int doubleClickSpeed = applause::Window::doubleClickSpeed();
    bool mainThreadSet = applause::Thread::main_thread_set_;
    std::thread::id mainThreadId = applause::Thread::main_thread_id_;
    std::unique_ptr<applause::Window> window =
        applause::createWindow(applause::Dimension::logicalPixels(240), applause::Dimension::logicalPixels(120));
    applause::ApplicationEditor editor;
};

}  // namespace

TEST_CASE("A select-on-focus field selects all on the click that focuses it, and later clicks place the caret",
          "[ui][texteditor]") {
    ClickableWindow app;
    applause::TextEditor field("Field");
    applause::TextEditor other("Other");
    field.setTextFieldEntry();
    other.setTextFieldEntry();
    field.setText("0123456789");
    app.editor.addChild(field);
    app.editor.addChild(other);
    field.setBounds(10, 10, 200, 30);
    other.setBounds(10, 60, 200, 30);

    int gained = 0;
    field.onFocusChange() += [&](bool focused, bool) {
        if (focused) ++gained;
    };

    app.click(field, field.width() - 4);
    REQUIRE(field.hasKeyboardFocus());
    REQUIRE(gained == 1);
    REQUIRE(field.selectionStart() == 0);
    REQUIRE(field.selectionEnd() == 10);

    // Already focused, so the click moves the caret to the start and selects nothing.
    app.click(field, 2);
    REQUIRE(gained == 1);
    REQUIRE(field.selectionStart() == 0);
    REQUIRE(field.selectionEnd() == 0);

    // Focusing it again by click selects all again.
    app.click(other, 4);
    REQUIRE_FALSE(field.hasKeyboardFocus());
    app.click(field, 2);
    REQUIRE(gained == 2);
    REQUIRE(field.selectionStart() == 0);
    REQUIRE(field.selectionEnd() == 10);
}
