#pragma once

#ifndef NDEBUG

#include <applause/ui/ApplauseUI.h>
#include <applause/ui/components/Button.h>

#include <memory>
#include <string>

namespace applause::inspector {

class InspectorWindow;

// Key/value list showing the selected frame's metadata. The bounds, visible,
// on-top and ignores-mouse rows are editable inline; the rest are read-only.
// Subscribes to InspectorWindow::onSelectionChanged() in the constructor and
// is also pulse-refreshed from InspectorWindow's polling timer so external
// mutations (resize, animation) propagate into the editors.
class InspectorPropertiesView : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorPropertiesKey);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorPropertiesValue);

    static constexpr float kRowHeight = 22.0f;
    static constexpr float kPaddingX = 8.0f;
    static constexpr float kKeyColumn = 90.0f;
    static constexpr float kFontSize = 11.0f;

    explicit InspectorPropertiesView(InspectorWindow& window);
    ~InspectorPropertiesView() override;

    void draw(applause::Canvas& canvas) override;
    void resized() override;

    // Re-reads the selected frame's bounds/flags into the editors and toggles,
    // skipping any TextEditor that currently has keyboard focus so the user's
    // in-progress edit isn't stomped. Hides the widgets when nothing is
    // selected.
    void refreshEditorsFromFrame();

private:
    void drawRow(applause::Canvas& canvas, int row, const char* key,
                 const std::string& value);
    void commitBoundsFromEditors();

    InspectorWindow& window_;
    applause::Font label_font_;

    std::unique_ptr<applause::TextEditor> x_editor_;
    std::unique_ptr<applause::TextEditor> y_editor_;
    std::unique_ptr<applause::TextEditor> w_editor_;
    std::unique_ptr<applause::TextEditor> h_editor_;

    std::unique_ptr<applause::ToggleTextButton> visible_toggle_;
    std::unique_ptr<applause::ToggleTextButton> on_top_toggle_;
    std::unique_ptr<applause::ToggleTextButton> ignores_mouse_toggle_;

    // Suppresses re-entrant onToggle handling while refreshEditorsFromFrame is
    // synchronising widget state from the underlying frame.
    bool suppress_toggle_callback_ = false;
};

}  // namespace applause::inspector

#endif  // NDEBUG
