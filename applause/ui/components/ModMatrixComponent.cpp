#include "ModMatrixComponent.h"

#include "applause/ui/ApplauseEditor.h"
#include "applause/ui/NativePopupMenu.h"

using namespace visage::dimension;

namespace applause {

VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixRowHeight, 30.0f);
VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixRowGap, 7.0f);
VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixPadding, 8.0f);
VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixColumnGap, 4.0f);
VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixToggleWidth, 30.0f);
VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixDeleteWidth, 30.0f);

ModMatrixComponent::PopupMenuComponent::PopupMenuComponent(const std::string& default_text) :
    UiButton(default_text), default_text_(default_text) {
    onToggle() += [this](Button*, bool) { showPopup(); };
}

void ModMatrixComponent::PopupMenuComponent::setItems(std::vector<std::pair<int, std::string>> items) {
    items_ = std::move(items);
}

void ModMatrixComponent::PopupMenuComponent::showPopup() {
    if (items_.empty()) return;

    auto* editor = findParent<applause::ApplauseEditor>();
    if (!editor) return;

    void* native_handle = editor->getNativeHandle();
    if (!native_handle) return;

    NativePopupMenu menu("Select");
    for (auto& [id, name] : items_) {
        menu.addOption(id, name);
        if (id == selected_id_) menu.select(true);
    }

    menu.onSelection() += [this](int id) {
        setSelectedId(id);
        on_item_selected_.callback(id);
    };

    auto pos = positionInWindow();
    menu.show(native_handle, pos.x, pos.y + height());
}

void ModMatrixComponent::PopupMenuComponent::setSelectedId(int id) {
    selected_id_ = id;
    for (auto& [item_id, name] : items_) {
        if (item_id == id) {
            selected_name_ = name;
            setText(name);
            return;
        }
    }
}

void ModMatrixComponent::PopupMenuComponent::reset() {
    selected_id_ = -1;
    selected_name_.clear();
    setText(default_text_);
}

ModMatrixComponent::Row::Row(ModMatrixComponent& owner, bool is_dummy) : owner_(owner), is_dummy_(is_dummy) {
    // Horizontal flex layout
    setFlexLayout(true);
    layout().setFlexRows(false);  // horizontal
    layout().setFlexGap(paletteValue(ApplauseModMatrixColumnGap));
    layout().setFlexItemAlignment(visage::Layout::ItemAlignment::Stretch);

    // Populate source menu
    std::vector<std::pair<int, std::string>> sources;
    for (uint16_t i = 0; i < owner_.matrix_.getSourceCount(); ++i)
        sources.emplace_back(i, owner_.matrix_.getSource(i).name);
    src_menu_.setItems(std::move(sources));

    // Populate destination menu
    std::vector<std::pair<int, std::string>> dests;
    for (uint16_t i = 0; i < owner_.matrix_.getDestinationCount(); ++i)
        dests.emplace_back(i, owner_.matrix_.getDestination(i).name);
    dst_menu_.setItems(std::move(dests));

    // Add children with flex properties
    addChild(&src_menu_);
    src_menu_.layout().setFlexGrow(1.0f);

    addChild(&dst_menu_);
    dst_menu_.layout().setFlexGrow(1.0f);

    addChild(&bipolar_toggle_);
    bipolar_toggle_.layout().setWidth(paletteValue(ApplauseModMatrixToggleWidth));
    bipolar_toggle_.layout().setFlexGrow(0.0f);
    bipolar_toggle_.layout().setFlexShrink(0.0f);

    addChild(&depth_slider_);
    depth_slider_.layout().setFlexGrow(1.5f);
    depth_slider_.setBipolar(true);

    addChild(&delete_button_);
    delete_button_.layout().setWidth(paletteValue(ApplauseModMatrixDeleteWidth));
    delete_button_.layout().setFlexGrow(0.0f);
    delete_button_.layout().setFlexShrink(0.0f);

    if (is_dummy) {
        setControlsActive(false);

        // When both source and destination are selected, activate
        src_menu_.on_item_selected_ += [this](int id) {
            src_idx_ = id;
            if (src_idx_ >= 0 && dst_idx_ >= 0) owner_.activateRow(this);
        };
        dst_menu_.on_item_selected_ += [this](int id) {
            dst_idx_ = id;
            if (src_idx_ >= 0 && dst_idx_ >= 0) owner_.activateRow(this);
        };
    }
}

void ModMatrixComponent::Row::bindToConnection(const ModConnection& conn) {
    is_dummy_ = false;
    src_idx_ = conn.src_idx;
    dst_idx_ = conn.dst_idx;

    src_menu_.setSelectedId(conn.src_idx);
    dst_menu_.setSelectedId(conn.dst_idx);
    depth_slider_.setValue(conn.getDepth());
    bipolar_toggle_.setToggled(conn.isBipolar());
    setControlsActive(true);

    // Wire active-row callbacks
    depth_slider_.on_value_changed += [this](float value) {
        if (auto* conn = owner_.matrix_.findConnection(src_idx_, dst_idx_)) conn->setDepth(value);
    };

    bipolar_toggle_.onToggle() += [this](Button*, bool on) {
        if (auto* conn = owner_.matrix_.findConnection(src_idx_, dst_idx_)) conn->setBipolar(on);
    };

    src_menu_.on_item_selected_ += [this](int id) {
        // Remove old connection, create new one with updated source
        auto* old_conn = owner_.matrix_.findConnection(src_idx_, dst_idx_);
        float depth = old_conn ? old_conn->getDepth() : 0.0f;
        bool bipolar = old_conn ? old_conn->isBipolar() : false;
        owner_.matrix_.removeConnection(src_idx_, dst_idx_);

        src_idx_ = id;
        auto& new_conn = owner_.matrix_.addConnection(owner_.matrix_.getSource(src_idx_),
                                                      owner_.matrix_.getDestination(dst_idx_), depth, bipolar);
    };

    dst_menu_.on_item_selected_ += [this](int id) {
        auto* old_conn = owner_.matrix_.findConnection(src_idx_, dst_idx_);
        float depth = old_conn ? old_conn->getDepth() : 0.0f;
        bool bipolar = old_conn ? old_conn->isBipolar() : false;
        owner_.matrix_.removeConnection(src_idx_, dst_idx_);

        dst_idx_ = id;
        auto& new_conn = owner_.matrix_.addConnection(owner_.matrix_.getSource(src_idx_),
                                                      owner_.matrix_.getDestination(dst_idx_), depth, bipolar);
    };

    delete_button_.onToggle() += [this](Button*, bool) { owner_.deleteRow(this); };
}

void ModMatrixComponent::Row::setControlsActive(bool active) {
    bipolar_toggle_.setActive(active);
    depth_slider_.setActive(active);
    delete_button_.setActive(active);
}

ModMatrixComponent::ModMatrixComponent(applause::ModMatrix& matrix) : matrix_(matrix) {
    scrollableLayout().setFlex(true);
    scrollableLayout().setFlexRows(true);  // vertical
    scrollableLayout().setFlexGap(paletteValue(ApplauseModMatrixRowGap));
    scrollableLayout().setPadding(paletteValue(ApplauseModMatrixPadding));
    scrollableLayout().setFlexItemAlignment(visage::Layout::ItemAlignment::Stretch);

    rebuildRows();
}

void ModMatrixComponent::rebuildRows() {
    // Remove all existing rows from scroll container
    for (auto& row : rows_) removeScrolledChild(row.get());
    rows_.clear();

    // Create rows for existing connections (skip depth mods)
    for (auto& conn : matrix_.getConnections()) {
        if (conn.isDepthMod()) continue;

        auto row = std::make_unique<Row>(*this, false);
        row->layout().setHeight(paletteValue(ApplauseModMatrixRowHeight));
        row->layout().setFlexGrow(0.0f);
        row->layout().setFlexShrink(0.0f);
        row->bindToConnection(conn);
        addScrolledChild(row.get());
        rows_.push_back(std::move(row));
    }

    addDummyRow();

    computeLayout();
    resized();
}

void ModMatrixComponent::resized() {
    ScrollableFrame::resized();
    computeLayout();

    float contentHeight = 0;
    for (auto& row : rows_) contentHeight = std::max(contentHeight, row->bottom());
    contentHeight += scrollableLayout().paddingBottom().amount;

    setScrollableHeight(contentHeight);
}

void ModMatrixComponent::addDummyRow() {
    auto row = std::make_unique<Row>(*this, true);
    row->layout().setHeight(paletteValue(ApplauseModMatrixRowHeight));
    row->layout().setFlexGrow(0.0f);
    row->layout().setFlexShrink(0.0f);

    addScrolledChild(row.get());
    rows_.push_back(std::move(row));
}

void ModMatrixComponent::activateRow(Row* row) {
    auto& conn = matrix_.addConnection(matrix_.getSource(row->srcIdx()), matrix_.getDestination(row->dstIdx()), 0.0f);
    row->bindToConnection(conn);
    addDummyRow();

    computeLayout();
    resized();
}

void ModMatrixComponent::deleteRow(Row* row) {
    matrix_.removeConnection(row->srcIdx(), row->dstIdx());
    removeScrolledChild(row);

    std::erase_if(rows_, [row](const std::unique_ptr<Row>& r) { return r.get() == row; });

    computeLayout();
    resized();
}

}  // namespace applause
