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
    static constexpr char32_t kTildeCharacter       = U'˜';
    static constexpr char32_t kUmlautCharacter      = U'¨';
    static constexpr char32_t kCircumflexCharacter  = U'ˆ';

    static bool isAlphaNumeric(char character) {
        return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z') ||
               (character >= '0' && character <= '9');
    }

    static bool isVariableCharacter(char character) {
        return isAlphaNumeric(character) || character == '_';
    }

    enum ActionState {
        kNone,
        kInserting,
        kDeleting,
    };

    enum class DeadKey {
        None,
        AcuteAccent,
        GraveAccent,
        Tilde,
        Umlaut,
        Circumflex
    };

    // Base colors. One-to-one with Visage's original TextEditor IDs, renamed
    // into the Applause namespace so the inspector groups them under this
    // class instead of colliding with Visage's "text_editor" group.
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorBackground);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorBorder);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorText);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorDefaultText);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorCaret);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorSelection);

    // State-variant colors. Default to the corresponding base color so visual
    // output is identical until a consumer customizes a variant. Precedence in
    // drawBackground / draw: disabled > focused > hover > base.
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorBackgroundFocused);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorBorderFocused);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorBackgroundHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorBorderHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorBackgroundDisabled);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorBorderDisabled);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseTextEditorTextDisabled);

    APPLAUSE_THEME_DEFINE_VALUE(ApplauseTextEditorRounding);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseTextEditorMarginX);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseTextEditorMarginY);
    // Previously hardcoded in drawBackground (2.0f) and drawSelection (1.0f).
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseTextEditorBorderWidth);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseTextEditorCaretWidth);

    explicit TextEditor(const std::string& name = "");
    ~TextEditor() override = default;

    auto& onTextChange() { return on_text_change_; }
    auto& onEnterKey() { return on_enter_key_; }
    auto& onEscapeKey() { return on_escape_key_; }

    // Virtual so subclasses can override individual draw phases without
    // reimplementing all of draw(). draw() is virtual via Frame.
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
        setBackgroundRounding(paletteValue(ApplauseTextEditorRounding));
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
        if (text_.justification() & (applause::Font::kLeft | applause::Font::kRight))
            return xMarginSize();
        return 0;
    }
    float yMargin() const {
        if (text_.justification() & applause::Font::kTop)
            return set_y_margin_ ? set_y_margin_ : paletteValue(ApplauseTextEditorMarginY);
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
            line_breaks_ = text_.font().lineBreaks(text_.text().c_str(), text_.text().length(),
                                                   width() - 2 * xMargin());
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
        if (multi_line)
            x_position_ = 0;
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

    // Per-instance color-ID redirects. The "main" setters (setBackgroundColorId,
    // setBorderColorId, setTextColorId) propagate to their state variants too,
    // so the common case — "this editor's background is just one color across
    // every state" — is a single call. Consumers who actually want a distinct
    // focused/hover/disabled appearance call the variant setter AFTER the main
    // setter; explicit variant routes override the propagated base ID.
    void setBackgroundColorId(visage::theme::ColorId id) {
        background_color_id_ = id;
        background_focused_color_id_ = id;
        background_hover_color_id_ = id;
        background_disabled_color_id_ = id;
    }
    void setBorderColorId(visage::theme::ColorId id) {
        border_color_id_ = id;
        border_focused_color_id_ = id;
        border_hover_color_id_ = id;
        border_disabled_color_id_ = id;
    }
    void setTextColorId(visage::theme::ColorId id) {
        text_color_id_ = id;
        text_disabled_color_id_ = id;
    }
    void setDefaultTextColorId(visage::theme::ColorId id)        { default_text_color_id_ = id; }
    void setCaretColorId(visage::theme::ColorId id)              { caret_color_id_ = id; }
    void setSelectionColorId(visage::theme::ColorId id)          { selection_color_id_ = id; }
    void setBackgroundFocusedColorId(visage::theme::ColorId id)  { background_focused_color_id_ = id; }
    void setBorderFocusedColorId(visage::theme::ColorId id)      { border_focused_color_id_ = id; }
    void setBackgroundHoverColorId(visage::theme::ColorId id)    { background_hover_color_id_ = id; }
    void setBorderHoverColorId(visage::theme::ColorId id)        { border_hover_color_id_ = id; }
    void setBackgroundDisabledColorId(visage::theme::ColorId id) { background_disabled_color_id_ = id; }
    void setBorderDisabledColorId(visage::theme::ColorId id)     { border_disabled_color_id_ = id; }
    void setTextDisabledColorId(visage::theme::ColorId id)       { text_disabled_color_id_ = id; }

protected:
    float xMarginSize() const {
        return set_x_margin_ ? set_x_margin_ : paletteValue(ApplauseTextEditorMarginX);
    }
    void addUndoPosition() { undo_history_.emplace_back(text_.text(), caret_position_); }

    // Resolve the background / border / text ColorIds for the current state
    // (active vs disabled vs focused vs hover). Centralized here so both
    // drawBackground and draw stay consistent and subclasses can override the
    // policy by overriding the draw helpers.
    visage::theme::ColorId resolvedBackgroundColorId() const {
        if (!active_)            return background_disabled_color_id_;
        if (hasKeyboardFocus())  return background_focused_color_id_;
        if (hovered_)            return background_hover_color_id_;
        return background_color_id_;
    }
    visage::theme::ColorId resolvedBorderColorId() const {
        if (!active_)            return border_disabled_color_id_;
        if (hasKeyboardFocus())  return border_focused_color_id_;
        if (hovered_)            return border_hover_color_id_;
        return border_color_id_;
    }
    visage::theme::ColorId resolvedTextColorId() const {
        return active_ ? text_color_id_ : text_disabled_color_id_;
    }

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

    visage::theme::ColorId background_color_id_           = ApplauseTextEditorBackground;
    visage::theme::ColorId border_color_id_               = ApplauseTextEditorBorder;
    visage::theme::ColorId text_color_id_                 = ApplauseTextEditorText;
    visage::theme::ColorId default_text_color_id_         = ApplauseTextEditorDefaultText;
    visage::theme::ColorId caret_color_id_                = ApplauseTextEditorCaret;
    visage::theme::ColorId selection_color_id_            = ApplauseTextEditorSelection;
    visage::theme::ColorId background_focused_color_id_   = ApplauseTextEditorBackgroundFocused;
    visage::theme::ColorId border_focused_color_id_       = ApplauseTextEditorBorderFocused;
    visage::theme::ColorId background_hover_color_id_     = ApplauseTextEditorBackgroundHover;
    visage::theme::ColorId border_hover_color_id_         = ApplauseTextEditorBorderHover;
    visage::theme::ColorId background_disabled_color_id_  = ApplauseTextEditorBackgroundDisabled;
    visage::theme::ColorId border_disabled_color_id_      = ApplauseTextEditorBorderDisabled;
    visage::theme::ColorId text_disabled_color_id_        = ApplauseTextEditorTextDisabled;

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
