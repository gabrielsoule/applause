#include "InspectorTreeView.h"

#ifndef NDEBUG

#include "FrameUtil.h"
#include "InspectorWindow.h"

#include <embedded/applause_fonts.h>

#include <cstdio>
#include <string>

namespace applause::inspector {

APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorTreeView, ApplauseInspectorRowSelectedBackground, 0x4466bbff);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorTreeView, ApplauseInspectorRowHoverBackground,    0x22ffffff);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorTreeView, ApplauseInspectorRowText,               0xffd8d8dc);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorTreeView, ApplauseInspectorRowTextMuted,          0xff7a7a80);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorTreeView, ApplauseInspectorRowAccent,             0xff66bbff);

namespace {

std::string frameKey(const applause::Frame* f) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "frame:%p", static_cast<const void*>(f));
    return buf;
}

// One row of the frame hierarchy. Wraps a Frame* and produces a stable key
// from its address. Click on the triangle (or on the already-selected row,
// matching the previous UX) toggles expand; click elsewhere selects.
class FrameTreeRow : public TreeRow {
public:
    FrameTreeRow(applause::Frame* frame, InspectorWindow& window)
        : frame_(frame), window_(window),
          label_font_(InspectorTreeView::kLabelFontSize,
                      applause::fonts::Jost_Regular_ttf) {}

    std::string stableKey() const override { return frameKey(frame_); }

    bool isBranch() const override {
        if (!frame_) return false;
        for (auto* c : frame_->children())
            if (!window_.isInternalFrame(c)) return true;
        return false;
    }

    std::vector<std::unique_ptr<TreeRow>> buildChildren() override {
        std::vector<std::unique_ptr<TreeRow>> out;
        if (!frame_) return out;
        for (auto* c : frame_->children()) {
            if (window_.isInternalFrame(c)) continue;
            out.push_back(std::make_unique<FrameTreeRow>(c, window_));
        }
        return out;
    }

    float headerHeight() const override { return InspectorTreeView::kRowHeightDefault; }

    void drawHeader(applause::Canvas& canvas,
                    applause::Bounds bounds,
                    const RowContext& ctx) override {
        if (!frame_) return;

        if (ctx.selected) {
            canvas.setColor(InspectorTreeView::ApplauseInspectorRowSelectedBackground);
            canvas.rectangle(0, 0, bounds.width(), bounds.height());
        } else if (ctx.hovered) {
            canvas.setColor(InspectorTreeView::ApplauseInspectorRowHoverBackground);
            canvas.rectangle(0, 0, bounds.width(), bounds.height());
        }

        const float tri_x = InspectorTreeView::kPaddingX
                          + ctx.depth * InspectorTreeView::kIndentPx;
        const float tri_cy = bounds.height() * 0.5f;
        const bool has_children = isBranch();

        if (has_children) {
            canvas.setColor(ctx.selected
                            ? InspectorTreeView::ApplauseInspectorRowText
                            : InspectorTreeView::ApplauseInspectorRowTextMuted);
            const float t = InspectorTreeView::kTriangleSize;
            if (ctx.expanded) {
                // Down-pointing triangle.
                canvas.triangle(tri_x,            tri_cy - t * 0.3f,
                                tri_x + t,        tri_cy - t * 0.3f,
                                tri_x + t * 0.5f, tri_cy + t * 0.5f);
            } else {
                // Right-pointing triangle.
                canvas.triangle(tri_x,            tri_cy - t * 0.5f,
                                tri_x + t * 0.8f, tri_cy,
                                tri_x,            tri_cy + t * 0.5f);
            }
        }

        canvas.setColor(ctx.selected
                        ? InspectorTreeView::ApplauseInspectorRowAccent
                        : InspectorTreeView::ApplauseInspectorRowText);
        const float text_x = tri_x + InspectorTreeView::kTriangleSize + 6.0f;
        canvas.text(applause::String(frameTypeName(*frame_)),
                    label_font_, applause::Font::kLeft,
                    text_x, 0,
                    bounds.width() - text_x - InspectorTreeView::kPaddingX,
                    bounds.height());
    }

    void onHeaderClick(applause::Point local, const RowContext& ctx) override {
        if (!frame_) return;
        // Triangle: pure expand/collapse, never changes selection.
        if (isBranch() && isOnTriangle(local, ctx.depth)) {
            ctx.toggleExpanded();
            return;
        }
        // Body click on the already-selected row toggles expansion. (We
        // can't route through selectFrame here because it short-circuits
        // when the frame is already selected — the auto-expand in the
        // selection handler would never run, so re-opening a closed row
        // would do nothing.)
        if (isBranch() && window_.selectedFrame() == frame_) {
            ctx.toggleExpanded();
            return;
        }
        window_.selectFrame(frame_);
    }

    applause::Frame* frame() const { return frame_; }

private:
    static bool isOnTriangle(applause::Point p, int depth) {
        const float tri_x = InspectorTreeView::kPaddingX
                          + depth * InspectorTreeView::kIndentPx;
        return p.x >= tri_x - 2.0f
            && p.x <= tri_x + InspectorTreeView::kTriangleSize + 4.0f;
    }

    applause::Frame* frame_;
    InspectorWindow& window_;
    applause::Font label_font_;
};

}  // namespace

InspectorTreeView::InspectorTreeView(InspectorWindow& window) : window_(window) {
    setName("InspectorTreeView");

    auto root = std::make_unique<FrameTreeRow>(&window_.editor(), window_);
    setRoot(std::move(root));
    // Editor root starts expanded so the user sees something on first open.
    expandKey(frameKey(&window_.editor()));

    window_.onSelectionChanged() += [this](applause::Frame* f) {
        // Auto-expand the selected frame itself so its children become
        // visible ("the dropdown opens"), plus expand all ancestors so the
        // row is reachable. Track whether anything changed — if so, scroll
        // to bring the selection into view.
        bool changed = false;
        if (f) {
            const std::string fkey = frameKey(f);
            if (!isExpanded(fkey)) { expandKey(fkey); changed = true; }
            for (auto* p = f->parent(); p; p = p->parent()) {
                const std::string pkey = frameKey(p);
                if (!isExpanded(pkey)) { expandKey(pkey); changed = true; }
            }
        }
        rebuild();
        TreeRow* row = f ? visibleRowForKey(frameKey(f)) : nullptr;
        setSelected(row);
        if (changed && row) scrollToRow(row);
    };

    window_.onTreeChanged() += [this] {
        rebuild();
        // If the selected frame was destroyed in the editor, the sweep in
        // rebuild() cleared our tree-side selection. Propagate the clear
        // back to the window so it doesn't keep holding a dangling Frame*.
        if (!selected() && window_.selectedFrame() != nullptr)
            window_.selectFrame(nullptr);
    };
}

InspectorTreeView::~InspectorTreeView() = default;

}  // namespace applause::inspector

#endif  // NDEBUG
