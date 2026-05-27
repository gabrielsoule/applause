#pragma once

#ifndef NDEBUG

#include <applause/ui/ApplauseUI.h>

#include <array>
#include <functional>
#include <memory>

namespace applause::inspector {

// Editor-side overlay: outlines the inspector's selected frame, draws
// dashed "distance to parent" lines (with numeric pixel labels) from each
// edge of the selection out to the corresponding parent edge, and places
// eight small drag-to-resize handles at the corners and edge midpoints of
// the selection.
//
// The overlay itself ignores mouse events and passes them through to the
// widgets underneath. Only the resize handles are interactive — they cover
// 10x10 px each, so the rest of the editor is fully clickable as normal.
class SelectionHighlightFrame : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorSelectionOutline);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorSelectionDistanceLine);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorSelectionDistanceText);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorResizeHandle);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseInspectorResizeHandleBorder);

    static constexpr float kThickness = 2.0f;
    static constexpr float kDashLength = 4.0f;
    static constexpr float kDashGap = 3.0f;
    static constexpr float kLabelFontSize = 10.0f;
    static constexpr float kMinGapForLabel = 14.0f;
    static constexpr float kHandleSize = 10.0f;
    static constexpr float kMinResizeSize = 4.0f;

    enum EdgeMask : unsigned {
        kLeft   = 1u << 0,
        kRight  = 1u << 1,
        kTop    = 1u << 2,
        kBottom = 1u << 3,
    };

    // Callback fires while the user drags a handle. `new_selected` is the
    // proposed selection rect in this frame's local coords (= editor coords,
    // because the highlight covers the whole editor).
    using OnResizeCallback = std::function<void(applause::Bounds new_selected)>;

    SelectionHighlightFrame();
    ~SelectionHighlightFrame() override;

    // Both rectangles are in this frame's local coord space.
    void setSelectionBounds(applause::Bounds selected, applause::Bounds parent);

    void setOnResize(OnResizeCallback cb) { on_resize_ = std::move(cb); }

    // Used by ResizeHandle children to fetch the live anchor rect and to
    // raise resize events back to the InspectorWindow.
    applause::Bounds selectedRect() const { return selected_; }
    void raiseResize(applause::Bounds new_selected) {
        if (on_resize_) on_resize_(new_selected);
    }

    void draw(applause::Canvas& canvas) override;

private:
    class ResizeHandle : public applause::Frame {
    public:
        ResizeHandle(SelectionHighlightFrame& owner, unsigned mask);

        void draw(applause::Canvas& canvas) override;
        void mouseEnter(const applause::MouseEvent& e) override;
        void mouseExit(const applause::MouseEvent& e) override;
        void mouseDown(const applause::MouseEvent& e) override;
        void mouseDrag(const applause::MouseEvent& e) override;

    private:
        SelectionHighlightFrame& owner_;
        const unsigned mask_;
        const applause::MouseCursor cursor_;
        applause::Point drag_start_window_;
        applause::Bounds drag_start_selected_;
    };

    void layoutHandles();

    applause::Bounds selected_;
    applause::Bounds parent_;
    applause::Font label_font_;
    OnResizeCallback on_resize_;
    std::array<std::unique_ptr<ResizeHandle>, 8> handles_;
};

}  // namespace applause::inspector

#endif  // NDEBUG
