#pragma once

#ifndef NDEBUG

#include "CollapsibleTreeView.h"

#include <memory>
#include <string>
#include <vector>

namespace applause { class ColorPicker; }

namespace applause::inspector {

class InspectorWindow;

// Theme browser: a CollapsibleTreeView over every color and value token.
// Top-level rows are groups (the part of a token's name before the dot);
// expanding a group reveals its colors then values. Edits go to the target:
// the selected frame, or the nearest frame above it that can hold theme
// values (the editor when nothing is selected), named in the top row.
// Clicking a color row reveals an inline ColorPicker beneath it;
// typing into a value row's text field sets it on the target. A row shows
// "reset" where the target sets its token, and otherwise what the token's
// default follows. Selecting a frame in the editor additively auto-expands
// any group whose name overlaps the frame's dynamic class name (Phase 2 —
// user's manual expansions are preserved).
class InspectorThemeView : public CollapsibleTreeView {
public:
    static inline const ColorToken kText{"inspectorThemeView.text", 0xffd8d8dc};
    static inline const ColorToken kTextMuted{"inspectorThemeView.textMuted", 0xff7a7a80};
    static inline const ColorToken kRowHover{"inspectorThemeView.rowHover", 0x22ffffff};
    static inline const ColorToken kRowSelected{"inspectorThemeView.rowSelected", 0x4466bbff};
    static inline const ColorToken kGroupBackgroundActive{"inspectorThemeView.groupBackgroundActive", 0xff262630};
    static inline const ColorToken kSwatchBorder{"inspectorThemeView.swatchBorder", 0xff3a3a40};
    static inline const ColorToken kPickerBackground{"inspectorThemeView.pickerBackground", 0xff141418};

    static constexpr float kHeaderHeight    = 22.0f;
    static constexpr float kRowHeight       = 20.0f;
    static constexpr float kPickerHeight    = 230.0f;
    static constexpr float kPickerPaddingY  = 4.0f;
    static constexpr float kSwatchSize      = 13.0f;
    static constexpr float kPaddingX        = 6.0f;
    static constexpr float kIndent          = 12.0f;
    static constexpr float kTriangleSize    = 6.0f;
    static constexpr float kLabelFontSize   = 11.0f;
    static constexpr float kValueEditorW    = 64.0f;

    explicit InspectorThemeView(InspectorWindow& window);
    ~InspectorThemeView() override;

    // Accessors used by the row adapters in the .cpp.
    InspectorWindow& window() { return window_; }
    const applause::Font& labelFont() const { return label_font_; }
    applause::ColorPicker* colorPicker() { return color_picker_.get(); }

    // "Editing color" state — at most one color row is open at a time. The
    // inline picker is reparented into whichever ThemeColorRow's stableKey
    // matches editingColor(). Click another color → setEditingColor moves the
    // picker; click the same one again → clearEditingColor closes it.
    const ColorToken* editingColor() const { return editing_color_; }
    void setEditingColor(const ColorToken& token);
    void clearEditingColor();

    // Called by ThemeValueRow when the user commits a number.
    void applyValue(const ValueToken& token, const std::string& text);

    // Where edits go: the selected frame, or the editor when nothing is
    // selected.
    applause::Frame& target() const;

    // Removes what the target sets the token to.
    void reset(const ColorToken& token);
    void reset(const ValueToken& token);

    // Phase-2 heuristic: which group names overlap this frame's class name.
    // Exposed for the group header to highlight "selection-relevant" groups.
    static std::vector<std::string> matchingGroupsFor(applause::Frame* frame);

private:
    void applyEditedColor(const applause::Color& color);
    // Shows the editing color's current value in the picker, without
    // reporting it back as an edit.
    void seedPicker();
    void applySelectionExpansion(applause::Frame* frame);

    InspectorWindow& window_;
    applause::Font label_font_;

    std::unique_ptr<applause::ColorPicker> color_picker_;
    bool suppress_picker_callback_ = false;
    const ColorToken* editing_color_ = nullptr;
};

}  // namespace applause::inspector

#endif  // NDEBUG
