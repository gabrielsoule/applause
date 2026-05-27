#include "CollapsibleTreeView.h"

#ifndef NDEBUG

#include <algorithm>
#include <utility>

namespace applause::inspector {

// Default click: branches toggle, leaves do nothing. Overridable.
void TreeRow::onHeaderClick(applause::Point /*local*/, const RowContext& ctx) {
    if (isBranch() && ctx.toggleExpanded) ctx.toggleExpanded();
}

// One visible row's backing Frame. Bound to a TreeRow* by the tree; consults
// it for height, drawing, layout, click dispatch. Lives in the tree's scroll
// container; pooled by TreeRow* in row_frames_.
class CollapsibleTreeView::RowFrame : public applause::Frame {
public:
    RowFrame(CollapsibleTreeView& tree, TreeRow& row) : tree_(tree), row_(&row) {
        onMouseEnter() += [this](const applause::MouseEvent&) {
            hovered_ = true;
            redraw();
        };
        onMouseExit() += [this](const applause::MouseEvent&) {
            hovered_ = false;
            redraw();
        };
        onMouseDown() += [this](const applause::MouseEvent& e) {
            if (!e.isLeftButton()) return;
            tree_.onRowClicked(row_, e.relativePosition());
        };
    }

    void bind(int depth, bool expanded, bool selected) {
        depth_ = depth;
        expanded_ = expanded;
        selected_ = selected;
    }

    void setSelected(bool s) {
        if (selected_ == s) return;
        selected_ = s;
        redraw();
    }

    TreeRow* row() const { return row_; }

    void draw(applause::Canvas& canvas) override {
        if (!row_) return;
        const float w = width();
        const float h = height();
        const float header_h = row_->headerHeight();
        const RowContext ctx = makeContext();

        row_->drawHeader(canvas, applause::Bounds(0, 0, w, header_h), ctx);

        // Detail strip visibility follows actual frame height — the tree's
        // walkModel already decided whether to allocate space.
        if (h > header_h + 0.5f) {
            row_->drawInlineDetail(
                canvas,
                applause::Bounds(0, header_h, w, h - header_h),
                ctx);
        }
    }

    void resized() override {
        if (!row_) return;
        relayoutContent();
    }

    void relayoutContent() {
        const float w = width();
        const float h = height();
        const float header_h = row_->headerHeight();
        const applause::Bounds header(0, 0, w, header_h);
        const bool show_detail = h > header_h + 0.5f;
        const applause::Bounds detail = show_detail
            ? applause::Bounds(0, header_h, w, h - header_h)
            : applause::Bounds(0, header_h, w, 0);
        row_->layoutContent(*this, header, detail, show_detail);
    }

private:
    RowContext makeContext() const {
        RowContext ctx;
        ctx.expanded = expanded_;
        ctx.selected = selected_;
        ctx.hovered = hovered_;
        ctx.depth = depth_;
        TreeRow* r = row_;
        CollapsibleTreeView* t = &tree_;
        const std::string key = r->stableKey();
        ctx.toggleExpanded = [t, key]() {
            t->toggleKey(key);
            t->rebuild();
        };
        ctx.select = [t, r]() { t->setSelected(r); };
        return ctx;
    }

    CollapsibleTreeView& tree_;
    TreeRow* row_ = nullptr;
    int depth_ = 0;
    bool expanded_ = false;
    bool selected_ = false;
    bool hovered_ = false;
};

CollapsibleTreeView::CollapsibleTreeView() = default;
CollapsibleTreeView::~CollapsibleTreeView() = default;

void CollapsibleTreeView::setRoot(std::unique_ptr<TreeRow> root) {
    // Drop all backing Frames before we discard their TreeRows. Frames
    // auto-detach in ~Frame but doing it here keeps ordering predictable.
    row_frames_.clear();
    row_pool_.clear();
    visible_.clear();
    visited_keys_.clear();
    expanded_keys_.clear();
    selected_ = nullptr;

    root_ = std::move(root);
    if (root_) root_->tree_ = this;
}

void CollapsibleTreeView::rebuild() {
    visible_.clear();
    visited_keys_.clear();

    if (root_) {
        float y = 0.0f;
        walkModel(*root_, 0, y);
    }

    sweepUnvisited();
    syncRowFrames();

    const float total = visible_.empty() ? 0.0f
                                         : visible_.back().y + visible_.back().h;
    setScrollableHeight(total);
    redraw();
}

void CollapsibleTreeView::walkModel(TreeRow& row, int depth, float& y) {
    // Intern this row by stableKey: if a pool entry exists, use it; if not,
    // treat the in-hand instance as canonical. Adoption into the pool is
    // handled by the caller (walkChildrenOf or setRoot).
    const std::string key = row.stableKey();
    visited_keys_.insert(key);

    auto it = row_pool_.find(key);
    TreeRow* canonical = (it != row_pool_.end()) ? it->second.get() : &row;
    canonical->tree_ = this;

    const bool is_branch = canonical->isBranch();
    const float header_h = canonical->headerHeight();
    // Branches: detail height is ignored — children fill the open space.
    // Leaves: the row's own inlineDetailHeight() drives both the height and
    // the "expanded" flag, so a leaf row can self-report "I'm showing
    // inline detail right now" without going through expanded_keys_.
    const float detail_h = is_branch ? 0.0f : canonical->inlineDetailHeight();
    const bool branch_expanded = is_branch && (expanded_keys_.count(key) > 0);
    const bool leaf_expanded = !is_branch && detail_h > 0.0f;

    Entry e;
    e.row = canonical;
    e.depth = depth;
    e.y = y;
    e.h = header_h + detail_h;
    e.expanded = is_branch ? branch_expanded : leaf_expanded;
    visible_.push_back(e);
    y += e.h;

    if (is_branch && branch_expanded) walkChildrenOf(*canonical, depth + 1, y);
}

void CollapsibleTreeView::walkChildrenOf(TreeRow& parent, int depth, float& y) {
    auto children = parent.buildChildren();
    for (auto& child_uptr : children) {
        const std::string child_key = child_uptr->stableKey();
        auto child_it = row_pool_.find(child_key);
        TreeRow* child_canonical;
        if (child_it != row_pool_.end()) {
            child_canonical = child_it->second.get();
            // Pool's instance wins; discard the freshly-built duplicate.
            child_uptr.reset();
        } else {
            child_canonical = child_uptr.get();
            row_pool_.emplace(child_key, std::move(child_uptr));
        }
        walkModel(*child_canonical, depth, y);
    }
}

void CollapsibleTreeView::sweepUnvisited() {
    // Remove RowFrames first (so their layout teardown runs before we drop
    // the TreeRow that owns any embedded widgets the RowFrame was hosting).
    for (auto it = row_frames_.begin(); it != row_frames_.end(); ) {
        const std::string key = it->first->stableKey();
        if (visited_keys_.count(key) == 0) it = row_frames_.erase(it);
        else                               ++it;
    }
    // Drop pool entries that weren't visited. Skip the root — it was passed
    // in by setRoot() and is not part of the pool.
    for (auto it = row_pool_.begin(); it != row_pool_.end(); ) {
        if (visited_keys_.count(it->first) == 0) it = row_pool_.erase(it);
        else                                     ++it;
    }
    // Clear selection if its row went away; notify so adapters can sync.
    if (selected_ && visited_keys_.count(selected_->stableKey()) == 0) {
        selected_ = nullptr;
        on_selection_changed_.callback(nullptr);
    }
}

void CollapsibleTreeView::syncRowFrames() {
    const float w = width();
    for (const Entry& e : visible_) {
        auto it = row_frames_.find(e.row);
        RowFrame* rf;
        if (it == row_frames_.end()) {
            auto frame = std::make_unique<RowFrame>(*this, *e.row);
            rf = frame.get();
            addScrolledChild(rf);
            row_frames_.emplace(e.row, std::move(frame));
        } else {
            rf = it->second.get();
        }
        rf->bind(e.depth, e.expanded, e.row == selected_);
        rf->setBounds(0, e.y, w, e.h);
        rf->relayoutContent();
    }
}

void CollapsibleTreeView::onRowClicked(TreeRow* row, applause::Point local) {
    if (!row) return;
    RowContext ctx;
    ctx.expanded = expanded_keys_.count(row->stableKey()) > 0;
    ctx.selected = (row == selected_);
    ctx.hovered = true;
    auto rf_it = row_frames_.find(row);
    if (rf_it != row_frames_.end()) {
        // Depth isn't critical for click handlers but pass it through.
        for (const Entry& e : visible_)
            if (e.row == row) { ctx.depth = e.depth; break; }
    }
    const std::string key = row->stableKey();
    CollapsibleTreeView* self = this;
    ctx.toggleExpanded = [self, key]() {
        self->toggleKey(key);
        self->rebuild();
    };
    TreeRow* r = row;
    ctx.select = [self, r]() { self->setSelected(r); };

    row->onHeaderClick(local, ctx);
}

void CollapsibleTreeView::setSelected(TreeRow* row) {
    if (selected_ == row) return;
    TreeRow* prev = selected_;
    selected_ = row;
    if (prev) {
        auto it = row_frames_.find(prev);
        if (it != row_frames_.end()) it->second->setSelected(false);
    }
    if (selected_) {
        auto it = row_frames_.find(selected_);
        if (it != row_frames_.end()) it->second->setSelected(true);
    }
    on_selection_changed_.callback(selected_);
}

void CollapsibleTreeView::expandKey(const std::string& key) {
    expanded_keys_.insert(key);
}

void CollapsibleTreeView::collapseKey(const std::string& key) {
    expanded_keys_.erase(key);
}

void CollapsibleTreeView::toggleKey(const std::string& key) {
    auto it = expanded_keys_.find(key);
    if (it == expanded_keys_.end()) expanded_keys_.insert(key);
    else                            expanded_keys_.erase(it);
}

bool CollapsibleTreeView::isExpanded(const std::string& key) const {
    return expanded_keys_.count(key) > 0;
}

TreeRow* CollapsibleTreeView::visibleRowForKey(const std::string& key) const {
    for (const Entry& e : visible_)
        if (e.row->stableKey() == key) return e.row;
    return nullptr;
}

void CollapsibleTreeView::scrollToRow(TreeRow* row) {
    if (!row) return;
    for (const Entry& e : visible_) {
        if (e.row == row) {
            setYPosition(std::max(0.0f, e.y - height() * 0.5f));
            return;
        }
    }
}

void CollapsibleTreeView::resized() {
    applause::ScrollableFrame::resized();
    // Update widths in place; y-positions don't change.
    const float w = width();
    for (const Entry& e : visible_) {
        auto it = row_frames_.find(e.row);
        if (it != row_frames_.end()) {
            it->second->setBounds(0, e.y, w, e.h);
            it->second->relayoutContent();
        }
    }
}

}  // namespace applause::inspector

#endif  // NDEBUG
