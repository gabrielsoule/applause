#pragma once

#ifndef NDEBUG

#include "CollapsibleTreeView.h"

#include <memory>
#include <string>
#include <vector>

namespace visage { class ColorPicker; }

namespace applause::inspector {

class InspectorWindow;

// Theme browser: a CollapsibleTreeView over the global ColorId / ValueId
// registry. Top-level rows are groups (one per source file the IDs live in);
// expanding a group reveals colors then values from that source. Clicking a
// color row reveals an inline visage::ColorPicker beneath it; typing into a
// value row's text field commits to the palette. Selecting a frame in the
// editor additively auto-expands any group whose name overlaps the frame's
// dynamic class name (Phase 2 — user's manual expansions are preserved).
class InspectorThemeView : public CollapsibleTreeView {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorThemeText);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorThemeTextMuted);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorThemeRowHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorThemeRowSelected);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorThemeGroupBackground);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorThemeGroupBackgroundActive);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorThemeSwatchBorder);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorThemePickerBackground);

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
    visage::ColorPicker* colorPicker() { return color_picker_.get(); }

    // "Editing color" state — at most one color row is open at a time. The
    // inline picker is reparented into whichever ThemeColorRow's stableKey
    // matches editingColor(). Click another color → setEditingColor moves the
    // picker; click the same one again → clearEditingColor closes it.
    visage::theme::ColorId editingColor() const { return editing_color_id_; }
    void setEditingColor(visage::theme::ColorId id);
    void clearEditingColor();

    // Called by ThemeValueRow when the user commits a number.
    void applyValue(visage::theme::ValueId id, const std::string& text);

    // Phase-2 heuristic: which group names overlap this frame's class name.
    // Exposed for the group header to highlight "selection-relevant" groups.
    static std::vector<std::string> matchingGroupsFor(applause::Frame* frame);

private:
    void applyEditedColor(const visage::Color& color);
    void applySelectionExpansion(applause::Frame* frame);

    InspectorWindow& window_;
    applause::Font label_font_;

    std::unique_ptr<visage::ColorPicker> color_picker_;
    bool suppress_picker_callback_ = false;
    visage::theme::ColorId editing_color_id_;
};

}  // namespace applause::inspector

#endif  // NDEBUG
