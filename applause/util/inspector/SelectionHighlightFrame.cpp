#include "SelectionHighlightFrame.h"

#ifndef NDEBUG

#include <embedded/applause_fonts.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace applause::inspector {

APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionHighlightFrame,
                               ApplauseInspectorSelectionOutline,      0xff66bbff);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionHighlightFrame,
                               ApplauseInspectorSelectionDistanceLine, 0xaa66bbff);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionHighlightFrame,
                               ApplauseInspectorSelectionDistanceText, 0xffe6f4ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionHighlightFrame,
                               ApplauseInspectorResizeHandle,          0xffffffff);
APPLAUSE_THEME_IMPLEMENT_COLOR(SelectionHighlightFrame,
                               ApplauseInspectorResizeHandleBorder,    0xff1a3a5a);

namespace {

bool boundsEqual(const applause::Bounds& a, const applause::Bounds& b) {
    return a.x() == b.x() && a.y() == b.y()
        && a.width() == b.width() && a.height() == b.height();
}

void drawHDash(applause::Canvas& canvas, float x_start, float x_end, float y) {
    if (x_start > x_end) std::swap(x_start, x_end);
    constexpr float kLen = SelectionHighlightFrame::kDashLength;
    constexpr float kGap = SelectionHighlightFrame::kDashGap;
    for (float x = x_start; x < x_end;) {
        const float seg_end = std::min(x + kLen, x_end);
        canvas.rectangle(x, y, seg_end - x, 1.0f);
        x = seg_end + kGap;
    }
}

void drawVDash(applause::Canvas& canvas, float y_start, float y_end, float x) {
    if (y_start > y_end) std::swap(y_start, y_end);
    constexpr float kLen = SelectionHighlightFrame::kDashLength;
    constexpr float kGap = SelectionHighlightFrame::kDashGap;
    for (float y = y_start; y < y_end;) {
        const float seg_end = std::min(y + kLen, y_end);
        canvas.rectangle(x, y, 1.0f, seg_end - y);
        y = seg_end + kGap;
    }
}

applause::MouseCursor cursorForMask(unsigned mask) {
    using EM = SelectionHighlightFrame;
    const bool L = mask & EM::kLeft;
    const bool R = mask & EM::kRight;
    const bool T = mask & EM::kTop;
    const bool B = mask & EM::kBottom;
    if ((L && T)) return applause::MouseCursor::TopLeftResize;
    if ((R && T)) return applause::MouseCursor::TopRightResize;
    if ((L && B)) return applause::MouseCursor::BottomLeftResize;
    if ((R && B)) return applause::MouseCursor::BottomRightResize;
    if (L || R)   return applause::MouseCursor::HorizontalResize;
    return applause::MouseCursor::VerticalResize;
}

// All 8 handle mask combinations, in a stable order so the array indices are
// meaningful (matches the order the handles are constructed and positioned).
constexpr unsigned kHandleMasks[8] = {
    SelectionHighlightFrame::kLeft  | SelectionHighlightFrame::kTop,
    SelectionHighlightFrame::kTop,
    SelectionHighlightFrame::kRight | SelectionHighlightFrame::kTop,
    SelectionHighlightFrame::kLeft,
    SelectionHighlightFrame::kRight,
    SelectionHighlightFrame::kLeft  | SelectionHighlightFrame::kBottom,
    SelectionHighlightFrame::kBottom,
    SelectionHighlightFrame::kRight | SelectionHighlightFrame::kBottom,
};

}  // namespace

SelectionHighlightFrame::ResizeHandle::ResizeHandle(SelectionHighlightFrame& owner,
                                                    unsigned mask)
    : owner_(owner), mask_(mask), cursor_(cursorForMask(mask)) {
    setName("InspectorResizeHandle");
    setOnTop(true);
    setIgnoresMouseEvents(false, false);
}

void SelectionHighlightFrame::ResizeHandle::mouseEnter(const applause::MouseEvent&) {
    // setCursorStyle is an immediate OS-level change, not a stored per-frame
    // setting, so it has to fire on enter/exit. Mouse capture during a drag
    // suppresses mouseExit on the captured handle, so the resize cursor stays
    // set for the full gesture.
    applause::setCursorStyle(cursor_);
}

void SelectionHighlightFrame::ResizeHandle::mouseExit(const applause::MouseEvent&) {
    applause::setCursorStyle(applause::MouseCursor::Arrow);
}

void SelectionHighlightFrame::ResizeHandle::draw(applause::Canvas& canvas) {
    canvas.setColor(owner_.ApplauseInspectorResizeHandle);
    canvas.rectangle(0, 0, width(), height());
    canvas.setColor(owner_.ApplauseInspectorResizeHandleBorder);
    canvas.rectangle(0,            0,             width(), 1.0f);
    canvas.rectangle(0,            height() - 1,  width(), 1.0f);
    canvas.rectangle(0,            0,             1.0f,    height());
    canvas.rectangle(width() - 1,  0,             1.0f,    height());
}

void SelectionHighlightFrame::ResizeHandle::mouseDown(const applause::MouseEvent& e) {
    drag_start_window_ = e.window_position;
    drag_start_selected_ = owner_.selectedRect();
}

void SelectionHighlightFrame::ResizeHandle::mouseDrag(const applause::MouseEvent& e) {
    const applause::Point delta = e.window_position - drag_start_window_;
    applause::Bounds b = drag_start_selected_;

    if (mask_ & kLeft) {
        float new_w = drag_start_selected_.width() - delta.x;
        float new_x = drag_start_selected_.x() + delta.x;
        if (new_w < kMinResizeSize) {
            new_x -= (kMinResizeSize - new_w);
            new_w = kMinResizeSize;
        }
        b.setX(new_x);
        b.setWidth(new_w);
    } else if (mask_ & kRight) {
        float new_w = drag_start_selected_.width() + delta.x;
        if (new_w < kMinResizeSize) new_w = kMinResizeSize;
        b.setWidth(new_w);
    }

    if (mask_ & kTop) {
        float new_h = drag_start_selected_.height() - delta.y;
        float new_y = drag_start_selected_.y() + delta.y;
        if (new_h < kMinResizeSize) {
            new_y -= (kMinResizeSize - new_h);
            new_h = kMinResizeSize;
        }
        b.setY(new_y);
        b.setHeight(new_h);
    } else if (mask_ & kBottom) {
        float new_h = drag_start_selected_.height() + delta.y;
        if (new_h < kMinResizeSize) new_h = kMinResizeSize;
        b.setHeight(new_h);
    }

    owner_.raiseResize(b);
}

SelectionHighlightFrame::SelectionHighlightFrame()
    : label_font_(kLabelFontSize, applause::fonts::Jost_Regular_ttf) {
    setName("InspectorSelectionHighlight");
    setIgnoresMouseEvents(true, true);
    setOnTop(true);

    for (std::size_t i = 0; i < handles_.size(); ++i) {
        handles_[i] = std::make_unique<ResizeHandle>(*this, kHandleMasks[i]);
        addChild(handles_[i].get());
        handles_[i]->setVisible(false);
    }
}

SelectionHighlightFrame::~SelectionHighlightFrame() = default;

void SelectionHighlightFrame::setSelectionBounds(applause::Bounds selected,
                                                 applause::Bounds parent) {
    if (boundsEqual(selected, selected_) && boundsEqual(parent, parent_)) return;
    selected_ = selected;
    parent_ = parent;
    layoutHandles();
    redraw();
}

void SelectionHighlightFrame::layoutHandles() {
    const bool show = selected_.width() > 0 && selected_.height() > 0;
    if (!show) {
        for (auto& h : handles_) h->setVisible(false);
        return;
    }

    const float half = kHandleSize * 0.5f;
    const float left   = selected_.x();
    const float right  = selected_.x() + selected_.width();
    const float top    = selected_.y();
    const float bottom = selected_.y() + selected_.height();
    const float cx     = selected_.x() + selected_.width()  * 0.5f;
    const float cy     = selected_.y() + selected_.height() * 0.5f;

    struct Anchor { float x, y; };
    const Anchor anchors[8] = {
        { left,  top    },  // kLeft  | kTop
        { cx,    top    },  // kTop
        { right, top    },  // kRight | kTop
        { left,  cy     },  // kLeft
        { right, cy     },  // kRight
        { left,  bottom },  // kLeft  | kBottom
        { cx,    bottom },  // kBottom
        { right, bottom },  // kRight | kBottom
    };

    for (std::size_t i = 0; i < handles_.size(); ++i) {
        handles_[i]->setBounds(anchors[i].x - half, anchors[i].y - half,
                               kHandleSize, kHandleSize);
        handles_[i]->setVisible(true);
    }
}

void SelectionHighlightFrame::draw(applause::Canvas& canvas) {
    if (selected_.width() <= 0 || selected_.height() <= 0) return;

    const float x = selected_.x();
    const float y = selected_.y();
    const float w = selected_.width();
    const float h = selected_.height();

    canvas.setColor(ApplauseInspectorSelectionOutline);
    canvas.rectangle(x, y, w, kThickness);
    canvas.rectangle(x, y + h - kThickness, w, kThickness);
    canvas.rectangle(x, y, kThickness, h);
    canvas.rectangle(x + w - kThickness, y, kThickness, h);

    const float p_left   = parent_.x();
    const float p_top    = parent_.y();
    const float p_right  = parent_.x() + parent_.width();
    const float p_bottom = parent_.y() + parent_.height();

    const float top_gap    = y - p_top;
    const float bottom_gap = p_bottom - (y + h);
    const float left_gap   = x - p_left;
    const float right_gap  = p_right - (x + w);

    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;

    canvas.setColor(ApplauseInspectorSelectionDistanceLine);
    if (top_gap    > 0.5f) drawVDash(canvas, p_top,    y,        cx);
    if (bottom_gap > 0.5f) drawVDash(canvas, y + h,    p_bottom, cx);
    if (left_gap   > 0.5f) drawHDash(canvas, p_left,   x,        cy);
    if (right_gap  > 0.5f) drawHDash(canvas, x + w,    p_right,  cy);

    canvas.setColor(ApplauseInspectorSelectionDistanceText);
    constexpr float kLabelW = 32.0f;
    constexpr float kLabelH = 12.0f;
    auto drawLabel = [&](float center_x, float center_y, float gap) {
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(std::round(gap)));
        canvas.text(applause::String(buf), label_font_, applause::Font::kCenter,
                    center_x - kLabelW * 0.5f, center_y - kLabelH * 0.5f,
                    kLabelW, kLabelH);
    };
    if (top_gap    >= kMinGapForLabel) drawLabel(cx, p_top + top_gap * 0.5f, top_gap);
    if (bottom_gap >= kMinGapForLabel) drawLabel(cx, (y + h) + bottom_gap * 0.5f, bottom_gap);
    if (left_gap   >= kMinGapForLabel) drawLabel(p_left + left_gap * 0.5f, cy, left_gap);
    if (right_gap  >= kMinGapForLabel) drawLabel((x + w) + right_gap * 0.5f, cy, right_gap);
}

}  // namespace applause::inspector

#endif  // NDEBUG
