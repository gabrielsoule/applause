#pragma once

#ifndef NDEBUG

#include <applause/ui/ApplauseUI.h>
#include <applause/ui/components/Button.h>
#include <applause/ui/components/Panel.h>

#include <memory>

namespace applause::inspector {

class InspectorTreeView;
class InspectorPropertiesView;
class InspectorThemeView;
class PickCaptureFrame;
class SelectionHighlightFrame;

// Standalone native OS window that hosts the Applause UI inspector. The
// inspector never appears inside the host plugin editor — it lives in its
// own top-level `visage::ApplicationWindow`. Closing the window via the
// native close button hides it entirely; next `toggleShown()` reopens.
//
// All inspector state (selected/hovered frame, pick mode flag, polling
// timer) lives here so the surrounding code only needs to deal with this
// one object.
class InspectorWindow : public applause::ApplicationWindow,
                        public applause::EventTimer {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorWindowBackground);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorWindowBorder);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorToolbarBackground);

    static constexpr int kPollIntervalMs = 100;
    static constexpr float kDefaultWidth = 1020.0f;
    static constexpr float kDefaultHeight = 620.0f;
    static constexpr float kToolbarHeight = 28.0f;
    static constexpr float kContentPadding = 6.0f;
    static constexpr float kColumnGap = 6.0f;
    // Three-column layout: tree | properties | theme. Ratios are fractions
    // of the column-content width (after subtracting padding + gaps).
    static constexpr float kTreeColumnRatio  = 0.28f;
    static constexpr float kPropsColumnRatio = 0.32f;

    explicit InspectorWindow(applause::Frame& editor);
    ~InspectorWindow() override;

    // Show / hide the native window. Construction does NOT show by default —
    // the editor stays untouched until the user toggles the inspector on.
    void toggleShown();
    void setShown(bool shown);
    bool isShown() const;

    void setPickMode(bool pick);
    void togglePickMode() { setPickMode(!pick_mode_); }
    bool isPickMode() const { return pick_mode_; }

    void selectFrame(applause::Frame* frame);
    applause::Frame* selectedFrame() const { return selected_; }

    applause::Frame* hoveredFrame() const { return hovered_; }

    applause::Frame& editor() { return editor_; }

    // The palette the inspector creates and attaches to the editor on first
    // show. Initialized lazily with defaults — until then, it is empty and
    // not attached. After attachment, `editor_.setPalette(&palette_)` makes
    // every theme lookup resolve through this palette instead of through the
    // theme-system defaults.
    applause::Palette& palette() { return palette_; }

    // True for frames the inspector itself parents onto the editor (the
    // transient pick-capture overlay and the selection-outline overlay). The
    // tree walk uses this to skip them, so they never appear as rows AND no
    // row ends up holding a pointer to one of them across destruction.
    bool isInternalFrame(const applause::Frame* f) const;

    // Called by PickCaptureFrame when the user moves the mouse / clicks while
    // pick mode is active. `point` is in editor (window) coordinates.
    void handlePickHover(applause::Point point);
    void handlePickCommit(applause::Point point);

    auto& onSelectionChanged() { return on_selection_changed_; }
    auto& onHoverChanged() { return on_hover_changed_; }
    auto& onPickModeChanged() { return on_pick_mode_changed_; }
    auto& onTreeChanged() { return on_tree_changed_; }

    void draw(applause::Canvas& canvas) override;
    void resized() override;
    bool keyPress(const applause::KeyEvent& e) override;
    void timerCallback() override;

private:
    void setHoveredFrame(applause::Frame* frame);
    void refreshSelectionHighlight();
    // Common cleanup invoked from both setShown(false) and onCloseRequested
    // so the native-close button doesn't leave the selection overlay attached
    // to the host editor.
    void detachOverlays();
    // Lazy palette initialization. Idempotent; first call populates the
    // palette from theme defaults and attaches it to the editor.
    void ensurePaletteAttached();

    applause::Frame& editor_;
    // Palette member must be declared BEFORE theme_panel_ / theme_ so that
    // the editors holding pointers into it are destroyed first.
    applause::Palette palette_;
    bool palette_attached_ = false;
    std::unique_ptr<applause::ToggleTextButton> pick_button_;
    // Panels host the tree / properties / theme as children of their content()
    // frames. Declared before the views so the views are destroyed first and
    // self-detach from the panel hierarchy cleanly.
    std::unique_ptr<applause::Panel> tree_panel_;
    std::unique_ptr<applause::Panel> properties_panel_;
    std::unique_ptr<applause::Panel> theme_panel_;
    std::unique_ptr<InspectorTreeView> tree_;
    std::unique_ptr<InspectorPropertiesView> properties_;
    std::unique_ptr<InspectorThemeView> theme_;
    std::unique_ptr<PickCaptureFrame> pick_capture_;
    std::unique_ptr<SelectionHighlightFrame> selection_highlight_;

    applause::Frame* selected_ = nullptr;
    applause::Frame* hovered_ = nullptr;
    bool pick_mode_ = false;
    int last_frame_count_ = 0;

    applause::CallbackList<void(applause::Frame*)> on_selection_changed_;
    applause::CallbackList<void(applause::Frame*)> on_hover_changed_;
    applause::CallbackList<void(bool)> on_pick_mode_changed_;
    applause::CallbackList<void()> on_tree_changed_;
};

}  // namespace applause::inspector

#endif  // NDEBUG
