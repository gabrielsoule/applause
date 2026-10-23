// Forked from Visage. Mostly a copy-paste clone, but with several improvements to theme editing and
// derived class behavior updates.
//
// See applause/ui/components/TextEditor.cpp for the full MIT license as per Visage.

#pragma once

#include <applause/ui/ApplauseUI.h>

#include <string>
#include <utility>
#include <vector>

namespace applause {

class TextEditor : public applause::ScrollableFrame {
public:
    static constexpr int kDefaultPasswordCharacter = '*';
    static constexpr int kMaxUndoHistory = 1000;

    static constexpr char32_t kAcuteAccentCharacter = U'´';
    static constexpr char32_t kGraveAccentCharacter = U'`';
    static constexpr char32_t kTildeCharacter = U'˜';
    static constexpr char32_t kUmlautCharacter = U'¨';
    static constexpr char32_t kCircumflexCharacter = U'ˆ';

    static bool isAlphaNumeric(char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
            (character >= '0' && character <= '9');
    }

    static bool isVariableCharacter(char character) { return isAlphaNumeric(character) || character == '_'; }

    enum ActionState {
        kNone,
        kInserting,
        kDeleting,
    };

    enum class DeadKey { None, AcuteAccent, GraveAccent, Tilde, Umlaut, Circumflex };

    static inline const ColorToken kBackground{"textEditor.background", 0x00000000};
    static inline const ColorToken kBorder{"textEditor.border", 0x00000000};
    static inline const ColorToken kText{"textEditor.text", 0xffffffff};
    static inline const ColorToken kDefaultText{"textEditor.defaultText", 0xff999999};
    static inline const ColorToken kCaret{"textEditor.caret", 0xffffffff};
    static inline const ColorToken kSelection{"textEditor.selection", 0x22ffffff};
    // State variants follow the base colors, so an editor only changes on focus, hover, or disable where a theme sets
    // them.
    static inline const ColorToken kBackgroundFocused{"textEditor.backgroundFocused", kBackground};
    static inline const ColorToken kBorderFocused{"textEditor.borderFocused", kBorder};
    static inline const ColorToken kBackgroundHover{"textEditor.backgroundHover", kBackground};
    static inline const ColorToken kBorderHover{"textEditor.borderHover", kBorder};
    static inline const ColorToken kBackgroundDisabled{"textEditor.backgroundDisabled", kBackground};
    static inline const ColorToken kBorderDisabled{"textEditor.borderDisabled", kBorder};
    static inline const ColorToken kTextDisabled{"textEditor.textDisabled", 0xff999999};

    static inline const ValueToken kRounding{"textEditor.rounding", 0.0f};
    static inline const ValueToken kMarginX{"textEditor.marginX", 9.0f};
    static inline const ValueToken kMarginY{"textEditor.marginY", 9.0f};
    static inline const ValueToken kBorderWidth{"textEditor.borderWidth", 0.0f};
    static inline const ValueToken kCaretWidth{"textEditor.caretWidth", 1.0f};

    explicit TextEditor(const std::string& name = "");
    ~TextEditor() override = default;

    auto& onTextChange() { return on_text_change_; }
    auto& onEnterKey() { return on_enter_key_; }
    auto& onEscapeKey() { return on_escape_key_; }

    virtual void drawBackground(applause::Canvas& canvas) const;
    void selectionRectangle(applause::Canvas& canvas, float x, float y, float w, float h) const;
    virtual void drawSelection(applause::Canvas& canvas) const;
    void draw(applause::Canvas& canvas) override;

    std::pair<float, float> indexToPosition(int index) const;
    std::pair<int, int> lineRange(int line) const;
    int positionToIndex(const std::pair<float, float>& position) const;

    void cancel();
    void deselect();
    void clear();
    void deleteSelected();
    void makeCaretVisible();
    void setViewBounds();
    int selectionStart() const { return std::min(caret_position_, selection_position_); }
    int selectionEnd() const { return std::max(caret_position_, selection_position_); }
    applause::String selection() const;

    int beginningOfWord() const;
    int endOfWord() const;

    void resized() override {
        applause::ScrollableFrame::resized();
        setBackgroundRounding(value(*this, kRounding));
        setLineBreaks();
        makeCaretVisible();
    }

    void dpiChanged() override {
        applause::Font f = font().withDpiScale(dpiScale());
        text_.setFont(f);
        default_text_.setFont(f);
    }

    void mouseEnter(const applause::MouseEvent& e) override;
    void mouseExit(const applause::MouseEvent& e) override;
    void mouseDown(const applause::MouseEvent& e) override;
    void mouseDrag(const applause::MouseEvent& e) override;
    void mouseUp(const applause::MouseEvent& e) override;
    void doubleClick(const applause::MouseEvent& e);
    void tripleClick(const applause::MouseEvent& e);
    bool handleDeadKey(const applause::KeyEvent& key);
    bool keyPress(const applause::KeyEvent& key) override;
    bool keyRelease(const applause::KeyEvent& key) override;
    bool receivesTextInput() override { return active_; }
    applause::String translateDeadKeyText(const applause::String& text) const;
    void textInput(const std::string& text) override;
    void focusChanged(bool is_focused, bool was_clicked) override;

    bool moveCaretLeft(bool modifier, bool shift);
    bool moveCaretRight(bool modifier, bool shift);

    void moveCaretVertically(bool shift, float y_offset);
    bool enterPressed();
    bool escapePressed();
    bool moveCaretUp(bool shift);
    bool moveCaretDown(bool shift);
    bool moveCaretToTop(bool shift);
    bool moveCaretToStartOfLine(bool shift);
    bool moveCaretToEnd(bool shift);
    bool moveCaretToEndOfLine(bool shift);
    bool pageUp(bool shift);
    bool pageDown(bool shift);
    bool copyToClipboard();
    bool cutToClipboard();
    bool pasteFromClipboard();
    bool deleteBackwards(bool modifier);
    bool deleteForwards(bool modifier);
    bool selectAll();
    bool undo();
    bool redo();

    void insertTextAtCaret(const applause::String& insert_text);

    void setBackgroundRounding(float rounding) {
        background_rounding_ = rounding;
        setScrollBarRounding(rounding);
    }
    void setMargin(float x, float y) {
        set_x_margin_ = x;
        set_y_margin_ = y;
    }

    float xMargin() const {
        if (text_.justification() & (applause::Font::kLeft | applause::Font::kRight)) return xMarginSize();
        return 0;
    }
    float yMargin() const {
        if (text_.justification() & applause::Font::kTop) return set_y_margin_ ? set_y_margin_ : value(*this, kMarginY);
        return 0;
    }

    void setPassword(int character = kDefaultPasswordCharacter) {
        text_.setCharacterOverride(character);
        if (character) {
            setMultiLine(false);
            setJustification(applause::Font::Justification::kLeft);
        }
    }

    void setLineBreaks() {
        if (text_.multiLine() && text_.font().packedFont()) {
            line_breaks_ =
                text_.font().lineBreaks(text_.text().c_str(), text_.text().length(), width() - 2 * xMargin());
        }
    }

    void setText(const applause::String& text) {
        if (max_characters_)
            text_.setText(text.substring(0, max_characters_));
        else
            text_.setText(text);
        caret_position_ = text_.text().length();
        selection_position_ = caret_position_;
        setLineBreaks();
        makeCaretVisible();
    }

    void setFilteredCharacters(const std::string& characters) { filtered_characters_ = characters; }
    void setDefaultText(const applause::String& default_text) { default_text_.setText(default_text); }
    void setMaxCharacters(int max) { max_characters_ = max; }
    void setMultiLine(bool multi_line) {
        text_.setMultiLine(multi_line);
        default_text_.setMultiLine(multi_line);
        if (multi_line) x_position_ = 0;
    }
    void setSelectOnFocus(bool select_on_focus) { select_on_focus_ = select_on_focus; }
    void setJustification(applause::Font::Justification justification) {
        text_.setJustification(justification);
        default_text_.setJustification(justification);
    }
    void setFont(const applause::Font& font) {
        applause::Font f = font.withDpiScale(dpiScale());
        text_.setFont(f);
        default_text_.setFont(f);
        setLineBreaks();
        makeCaretVisible();
    }
    void setActive(bool active) { active_ = active; }
    void setNumberEntry();
    void setTextFieldEntry();

    const applause::String& text() const { return text_.text(); }
    int textLength() const { return text_.text().length(); }
    const applause::Font& font() const { return text_.font(); }
    applause::Font::Justification justification() const { return text_.justification(); }

protected:
    float xMarginSize() const { return set_x_margin_ ? set_x_margin_ : value(*this, kMarginX); }
    void addUndoPosition() { undo_history_.emplace_back(text_.text(), caret_position_); }

    // The background / border / text tokens for the current state
    // (active vs disabled vs focused vs hover). Centralized here so both
    // drawBackground and draw stay consistent and subclasses can override the
    // policy by overriding the draw helpers.
    const ColorToken& backgroundToken() const {
        if (!active_) return kBackgroundDisabled;
        if (hasKeyboardFocus()) return kBackgroundFocused;
        if (hovered_) return kBackgroundHover;
        return kBackground;
    }
    const ColorToken& borderToken() const {
        if (!active_) return kBorderDisabled;
        if (hasKeyboardFocus()) return kBorderFocused;
        if (hovered_) return kBorderHover;
        return kBorder;
    }
    const ColorToken& textToken() const { return active_ ? kText : kTextDisabled; }

    applause::CallbackList<void()> on_text_change_;
    applause::CallbackList<void()> on_enter_key_;
    applause::CallbackList<void()> on_escape_key_;

    DeadKey dead_key_entry_ = DeadKey::None;
    applause::Text text_;
    applause::Text default_text_;
    std::string filtered_characters_;
    std::vector<int> line_breaks_;
    int caret_position_ = 0;
    int selection_position_ = 0;
    std::pair<float, float> selection_start_point_;
    std::pair<float, float> selection_end_point_;
    int max_characters_ = 0;

    bool select_on_focus_ = false;
    bool mouse_focus_ = false;
    bool hovered_ = false;
    bool active_ = true;

    float background_rounding_ = 1.0f;
    float set_x_margin_ = 0.0f;
    float set_y_margin_ = 0.0f;
    float x_position_ = 0.0f;

    ActionState action_state_ = kNone;
    std::vector<std::pair<applause::String, int>> undo_history_;
    std::vector<std::pair<applause::String, int>> undone_history_;

    APPLAUSE_LEAK_CHECKER(TextEditor)
};

}  // namespace applause
