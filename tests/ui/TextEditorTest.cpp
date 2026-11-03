// The tests after the first one are from Visage's text editor tests (licenses/Visage-MIT.txt).

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <applause/ui/ApplicationEditor.h>
#include <applause/ui/components/TextEditor.h>
#include <applause/util/ThreadUtils.h>
#include <applause/windowing/Windowing.h>

#include <memory>
#include <thread>

using namespace applause;

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

TEST_CASE("Text editor text input callbacks", "[widgets]") {
    TextEditor editor;
    int text_change_count = 0;
    std::string input_text;
    editor.onTextChange() += [&] { text_change_count++; };
    editor.onTextInput() += [&](const std::string& text) { input_text = text; };

    editor.processTextInput("a");
    REQUIRE(editor.text() == "a");
    REQUIRE(text_change_count == 1);
    REQUIRE(input_text == "a");
}

TEST_CASE("Text editor deleted by its text change handler", "[widgets]") {
    TextEditor* editor = new TextEditor();
    editor->setText("abc");
    editor->onTextChange() += [&editor] {
        delete editor;
        editor = nullptr;
    };

    SECTION("Paste") { editor->processKeyPress(KeyEvent(KeyCode::V, kModifierRegCtrl | kModifierCmd, true)); }
    SECTION("Text input") { editor->processTextInput("d"); }

    CHECK(editor == nullptr);
}

TEST_CASE("Text editor only consumes releases of keys it consumed", "[widgets]") {
    TextEditor editor;
    editor.setText("abc");
    editor.processFocusChanged(true, false);

    REQUIRE(editor.processKeyPress(KeyEvent(KeyCode::A, 0, true)));
    CHECK(editor.processKeyRelease(KeyEvent(KeyCode::A, 0, false)));

    REQUIRE_FALSE(editor.processKeyPress(KeyEvent(KeyCode::LCtrl, kModifierRegCtrl, true)));
    CHECK_FALSE(editor.processKeyRelease(KeyEvent(KeyCode::LCtrl, 0, false)));

    REQUIRE_FALSE(editor.processKeyPress(KeyEvent(KeyCode::F5, kModifierRegCtrl, true)));
    CHECK_FALSE(editor.processKeyRelease(KeyEvent(KeyCode::F5, kModifierRegCtrl, false)));

    CHECK_FALSE(editor.processKeyRelease(KeyEvent(KeyCode::B, 0, false)));
}

TEST_CASE("Text editor deleted by its Enter or Escape handler", "[widgets]") {
    KeyCode key_code = GENERATE(KeyCode::Return, KeyCode::Escape);
    TextEditor* editor = new TextEditor();
    editor->processFocusChanged(true, false);
    auto remove = [&editor] {
        delete editor;
        editor = nullptr;
    };
    editor->onEnterKey() = remove;
    editor->onEscapeKey() = remove;

    editor->processKeyPress(KeyEvent(key_code, 0, true));
    CHECK(editor == nullptr);
}

TEST_CASE("Text editor doesn't record a key whose press moves focus away", "[widgets]") {
    TextEditor editor;
    editor.processFocusChanged(true, false);
    editor.onEnterKey() = [&editor] { editor.processFocusChanged(false, false); };

    REQUIRE(editor.processKeyPress(KeyEvent(KeyCode::Return, 0, true)));
    editor.processFocusChanged(true, false);
    CHECK_FALSE(editor.processKeyRelease(KeyEvent(KeyCode::Return, 0, false)));
}

#if APPLAUSE_MAC
TEST_CASE("Text editor dead key at max characters", "[widgets]") {
    TextEditor editor;
    editor.setMaxCharacters(3);
    editor.setText("abc");

    editor.moveCaretToTop(false);
    editor.processKeyPress(KeyEvent(KeyCode::E, kModifierAlt, true));
    editor.processTextInput("e");
    REQUIRE(editor.text() == "abc");

    editor.moveCaretToEnd(false);
    editor.processKeyPress(KeyEvent(KeyCode::E, kModifierAlt, true));
    editor.processTextInput("e");
    REQUIRE(editor.text() == "abc");
}

TEST_CASE("Text editor dead key with filtered accent", "[widgets]") {
    TextEditor editor;
    editor.setFilteredCharacters("´");
    editor.setText("abc");

    editor.moveCaretToEnd(false);
    editor.processKeyPress(KeyEvent(KeyCode::E, kModifierAlt, true));
    editor.processTextInput("e");
    REQUIRE(editor.text() == "abce");
}

TEST_CASE("Text editor dead key", "[widgets]") {
    TextEditor editor;
    editor.setText("ab");

    editor.moveCaretToEnd(false);
    editor.processKeyPress(KeyEvent(KeyCode::E, kModifierAlt, true));
    editor.processTextInput("e");
    REQUIRE(editor.text() == "abé");
}
#endif

TEST_CASE("Text editor switching off multi-line clears line breaks", "[widgets]") {
    TextEditor editor;
    editor.setBounds(0, 0, 60, 200);
    editor.setMultiLine(true);
    editor.setText("one two three four five six seven eight");
    REQUIRE(editor.lineRange(1).first > 0);

    editor.setMultiLine(false);
    editor.setText("");
    REQUIRE(editor.lineRange(0) == std::pair<int, int>(0, 0));
}

TEST_CASE("Text editor line range past the last line", "[widgets]") {
    TextEditor editor;
    editor.setText("abc");
    REQUIRE(editor.lineRange(5) == std::pair<int, int>(0, 3));
}

TEST_CASE("Text editor inactive", "[widgets]") {
    TextEditor editor;
    editor.processTextInput("abc");
    editor.setActive(false);

    editor.processKeyPress(KeyEvent(KeyCode::Backspace, 0, true));
    editor.moveCaretToTop(false);
    editor.processKeyPress(KeyEvent(KeyCode::Delete, 0, true));
    editor.processKeyPress(KeyEvent(KeyCode::E, kModifierAlt, true));
    editor.processKeyPress(KeyEvent(KeyCode::Z, kModifierRegCtrl, true));
    REQUIRE(editor.text() == "abc");
}

TEST_CASE("Text editor single line input with newlines", "[widgets]") {
    TextEditor editor;
    editor.setMultiLine(false);
    editor.processTextInput("a\r\nb\nc");
    REQUIRE(editor.text() == "abc");

    editor.setMultiLine(true);
    editor.processTextInput("d\ne");
    REQUIRE(editor.text() == "abcd\ne");
}

TEST_CASE("Text editor end of a soft wrapped line keeps the caret on that line", "[widgets]") {
    TextEditor editor;
    editor.setBounds(0, 0, 60, 200);
    editor.setMultiLine(true);
    editor.setText("one two three four five six seven eight");
    std::pair<int, int> first_line = editor.lineRange(0);
    REQUIRE(first_line.second < editor.text().length());
    float first_line_y = editor.indexToPosition(0).second;

    editor.moveCaretToTop(false);
    editor.processKeyPress(KeyEvent(KeyCode::End, 0, true));
    int caret = editor.selectionStart();
    REQUIRE(caret == first_line.second - 1);
    REQUIRE(editor.indexToPosition(caret).second == first_line_y);

    REQUIRE(editor.positionToIndex({1000.0f, first_line_y}) == caret);
}

TEST_CASE("Text editor deleting nothing leaves the text and undo history alone", "[widgets]") {
    TextEditor editor;
    editor.setText("abc");
    int changes = 0;
    editor.onTextChange() += [&changes] { changes++; };

    editor.processTextInput("d");
    editor.undo();
    REQUIRE(editor.text() == "abc");
    REQUIRE(changes == 2);

    editor.moveCaretToEnd(false);
    editor.deleteForwards(false);
    editor.moveCaretToTop(false);
    editor.deleteBackwards(false);
    editor.cutToClipboard();
    REQUIRE(editor.text() == "abc");
    REQUIRE(changes == 2);

    editor.redo();
    REQUIRE(editor.text() == "abcd");
}

TEST_CASE("Text editor word deletion handles non-ASCII letters and runs of spaces", "[widgets]") {
    TextEditor editor;
    editor.setText("one naïve");
    editor.deleteBackwards(true);
    REQUIRE(editor.text() == "one ");

    editor.setText("one   two");
    editor.deleteBackwards(true);
    REQUIRE(editor.text() == "one   ");
    editor.deleteBackwards(true);
    REQUIRE(editor.text() == "");

    editor.setText("one   two");
    editor.moveCaretToTop(false);
    editor.deleteForwards(true);
    REQUIRE(editor.text() == "   two");
    editor.deleteForwards(true);
    REQUIRE(editor.text() == "");
}

TEST_CASE("Text editor undo history is limited", "[widgets]") {
    TextEditor editor;
    for (int i = 0; i < 600; ++i) {
        editor.processTextInput("a");
        editor.deleteBackwards(false);
    }

    int undos = 0;
    editor.onTextChange() += [&undos] { undos++; };
    for (int i = 0; i < 2 * TextEditor::kMaxUndoHistory; ++i) editor.undo();
    REQUIRE(undos == TextEditor::kMaxUndoHistory);
}

TEST_CASE("Text editor keeps undo and redo keys when there's nothing to undo or redo", "[widgets]") {
    TextEditor editor;
    editor.processFocusChanged(true, false);
    REQUIRE(editor.undo());
    REQUIRE(editor.redo());
    REQUIRE(editor.processKeyPress(KeyEvent(KeyCode::Z, kModifierRegCtrl | kModifierCmd, true)));
    REQUIRE(editor.processKeyPress(KeyEvent(KeyCode::Z, kModifierRegCtrl | kModifierCmd | kModifierShift, true)));
    REQUIRE(editor.processKeyPress(KeyEvent(KeyCode::Y, kModifierRegCtrl | kModifierCmd, true)));
    REQUIRE(editor.text().isEmpty());

    editor.processTextInput("a");
    REQUIRE(editor.processKeyPress(KeyEvent(KeyCode::Z, kModifierRegCtrl | kModifierCmd, true)));
    REQUIRE(editor.text().isEmpty());
    REQUIRE(editor.processKeyPress(KeyEvent(KeyCode::Z, kModifierRegCtrl | kModifierCmd, true)));
    REQUIRE(editor.processKeyPress(KeyEvent(KeyCode::Y, kModifierRegCtrl | kModifierCmd, true)));
    REQUIRE(editor.text() == "a");
    REQUIRE(editor.processKeyPress(KeyEvent(KeyCode::Y, kModifierRegCtrl | kModifierCmd, true)));
    REQUIRE(editor.text() == "a");
}

#if !APPLAUSE_MAC
TEST_CASE("Text editor leaves Alt+keys to its parent", "[widgets]") {
    TextEditor editor;
    editor.setText("abc");
    editor.processFocusChanged(true, false);
    editor.moveCaretToEnd(false);

    REQUIRE_FALSE(editor.processKeyPress(KeyEvent(KeyCode::E, kModifierAlt, true)));
    REQUIRE_FALSE(editor.processKeyPress(KeyEvent(KeyCode::A, kModifierAlt, true)));
    REQUIRE_FALSE(editor.processKeyPress(KeyEvent(KeyCode::Left, kModifierAlt, true)));
    REQUIRE(editor.text() == "abc");

    editor.processTextInput("e");
    REQUIRE(editor.text() == "abce");
}
#endif

TEST_CASE("Text editor setText clears the undo history", "[widgets]") {
    TextEditor editor;
    editor.processTextInput("typed");
    editor.setText("replaced");
    editor.undo();
    REQUIRE(editor.text() == "replaced");
    editor.redo();
    REQUIRE(editor.text() == "replaced");
}
