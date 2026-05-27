#pragma once

#ifndef NDEBUG

#include <applause/ui/ApplauseUI.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace applause::inspector {

class CollapsibleTreeView;

// Per-row context handed to TreeRow callbacks. Holds the bound state and
// closures the row can use to mutate selection / expansion without knowing
// about the tree view directly.
struct RowContext {
    bool expanded = false;
    bool selected = false;
    bool hovered = false;
    int depth = 0;
    std::function<void()> toggleExpanded;
    std::function<void()> select;
};

// Adapter for one logical row in a CollapsibleTreeView. Subclasses describe
// what a row looks like, whether it's a branch, what its key is, and how it
// reacts to clicks. The tree owns the corresponding visible Frame and calls
// back into the row for draw/layout/click dispatch.
//
// Lifetime: rows are stored in CollapsibleTreeView's pool keyed by
// stableKey(). On rebuild, surviving rows (same key) are reused, so any
// per-row state (an inline ColorPicker, an open editing flag, etc.) persists
// across rebuilds.
class TreeRow {
public:
    virtual ~TreeRow() = default;

    // Identity. Must be stable across rebuilds for the same logical row so
    // that pool lookups and expanded_keys_ continue to work after a tree-
    // shape change. Examples: "frame:0x7f...", "group:Knob", "color:42".
    virtual std::string stableKey() const = 0;

    // Structure. Branches return non-empty children when asked, but only get
    // asked if their stableKey() is in the tree's expanded set.
    virtual bool isBranch() const { return false; }
    virtual std::vector<std::unique_ptr<TreeRow>> buildChildren() { return {}; }

    // Height. Total row height = headerHeight() + (expanded ? inlineDetailHeight() : 0).
    // Non-zero inlineDetailHeight is for leaves that grow when expanded (e.g.
    // color row revealing an inline picker).
    virtual float headerHeight() const { return 20.0f; }
    virtual float inlineDetailHeight() const { return 0.0f; }

    // Drawing. `bounds` is row-local; the canvas region has already been
    // beginRegion'd by the host Frame.
    virtual void drawHeader(applause::Canvas& canvas,
                            applause::Bounds bounds,
                            const RowContext& ctx) = 0;
    virtual void drawInlineDetail(applause::Canvas& /*canvas*/,
                                  applause::Bounds /*bounds*/,
                                  const RowContext& /*ctx*/) {}

    // Hook for adapters that parent child Frames (text editors, color
    // pickers, ...) inside the row's Frame. Called by the tree after the
    // row's bounds are set, every layout. `expanded` reflects current state.
    // `header` and `detail` are row-local rects (detail.height() == 0 when
    // not expanded). Default does nothing.
    virtual void layoutContent(applause::Frame& /*row_frame*/,
                               applause::Bounds /*header*/,
                               applause::Bounds /*detail*/,
                               bool /*expanded*/) {}

    // Mouse routing (row-local coords). Tree calls this after hit-testing the
    // row's Frame; the row decides whether to toggle expansion, select, etc.
    // Default behavior: branches toggle on any click; leaves do nothing.
    virtual void onHeaderClick(applause::Point local, const RowContext& ctx);

    CollapsibleTreeView* tree() { return tree_; }

protected:
    friend class CollapsibleTreeView;
    CollapsibleTreeView* tree_ = nullptr;
};

// Scrollable tree component. Each visible row is backed by an internal
// RowFrame (real Visage Frame child of the scroll container) so hover,
// per-row redraws, and child-widget reparenting come from Visage for free.
//
// Usage:
//   auto tree = std::make_unique<CollapsibleTreeView>();
//   tree->setRoot(std::make_unique<MyRootRow>(...));
//   tree->rebuild();                                       // populate
//   tree->onSelectionChanged() += [](TreeRow* r) { ... };  // observe
//
// The root row itself is conventionally a branch whose buildChildren()
// returns the top-level rows. Whether the root's own header is shown depends
// on whether the consumer wants it — see drawHeader() / headerHeight() on
// the root adapter.
class CollapsibleTreeView : public applause::ScrollableFrame {
public:
    CollapsibleTreeView();
    ~CollapsibleTreeView() override;

    // Install a new root. Resets pool, expansion state, and selection.
    void setRoot(std::unique_ptr<TreeRow> root);

    // Recompute the visible flat list from the model + expanded keys and lay
    // out RowFrames. Reuses pool entries by stableKey() so per-row state
    // survives. Always call after mutating expand state or model.
    void rebuild();

    // Selection. selected() may be nullptr.
    void setSelected(TreeRow* row);
    TreeRow* selected() const { return selected_; }
    auto& onSelectionChanged() { return on_selection_changed_; }

    // Expansion. expandKey() is additive (won't collapse anything else);
    // collapseKey() removes; isExpanded() queries. None of these trigger
    // rebuild — callers batch and call rebuild() explicitly.
    void expandKey(const std::string& key);
    void collapseKey(const std::string& key);
    void toggleKey(const std::string& key);
    bool isExpanded(const std::string& key) const;

    // Visible row currently bound to the given key, or nullptr if not in the
    // visible set. Useful for "scroll to me" after an adapter expands the
    // path to a key.
    TreeRow* visibleRowForKey(const std::string& key) const;

    // Scrolls so that `row` is roughly centered in the view. No-op if `row`
    // isn't currently visible.
    void scrollToRow(TreeRow* row);

    void resized() override;

private:
    friend class RowFrame;

    struct Entry {
        TreeRow* row = nullptr;
        int depth = 0;
        float y = 0.0f;
        float h = 0.0f;
        bool expanded = false;  // branch: in expanded_keys_; leaf: detail showing
    };

    // Walks the model from root, recursing into branches whose stableKey()
    // is in expanded_keys_, and appends Entries to visible_ with cumulative
    // y. New TreeRows are interned into row_pool_ on first sight; existing
    // ones are reused and the freshly-built duplicates dropped.
    void walkModel(TreeRow& row, int depth, float& y);
    void walkChildrenOf(TreeRow& parent, int depth, float& y);

    // Ensures a RowFrame exists for each visible Entry, attaches/detaches
    // from the scroll container, sets bounds, and dispatches layoutContent.
    void syncRowFrames();

    // Drops any RowFrame / pool TreeRow whose key wasn't visited this rebuild.
    void sweepUnvisited();

    // Mouse dispatch from a RowFrame back into its TreeRow.
    void onRowClicked(TreeRow* row, applause::Point local_to_row);

    // Re-broadcast on_selection_changed_ + repaint affected rows. Internal
    // for setSelected and for selection driven by row callbacks.
    void applySelection(TreeRow* row);

    std::unique_ptr<TreeRow> root_;
    std::unordered_set<std::string> expanded_keys_;
    std::unordered_map<std::string, std::unique_ptr<TreeRow>> row_pool_;

    // Visible rows in scroll order, regenerated each rebuild().
    std::vector<Entry> visible_;

    // Per-TreeRow Frame backing. Created lazily on first visibility, reused
    // across rebuilds, removed when the row is swept from the pool.
    class RowFrame;
    std::unordered_map<TreeRow*, std::unique_ptr<RowFrame>> row_frames_;

    // Set during walkModel so sweepUnvisited can find pool entries that
    // weren't touched this rebuild.
    std::unordered_set<std::string> visited_keys_;

    TreeRow* selected_ = nullptr;
    applause::CallbackList<void(TreeRow*)> on_selection_changed_;
};

}  // namespace applause::inspector

#endif  // NDEBUG
