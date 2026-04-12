#include "ModMatrixComponent.h"

#include <embedded/applause_fonts.h>

#include "applause/ui/ApplauseEditor.h"
#include "applause/ui/NativePopupMenu.h"

using namespace visage::dimension;

namespace applause {

VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixRowHeight, 30.0f);    // height of each row
VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixRowGap, 7.0f);       // vertical spacing between rows
VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixPadding, 20.0f);      // outer padding of the matrix
VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixColumnGap, 16.0f);    // horizontal gap between columns
VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixToggleWidth, 30.0f); // bipolar toggle button width
VISAGE_THEME_IMPLEMENT_VALUE(ModMatrixComponent, ApplauseModMatrixDeleteWidth, 30.0f); // delete button width

static std::string connectionLabel(const ModMatrix& matrix, const ModConnection& conn) {
    return matrix.getSource(conn.src_idx).name + " -> " + conn.destination()->name;
}

ModMatrixComponent::Row::Row(ModMatrixComponent& owner, bool is_dummy) : owner_(owner), is_dummy(is_dummy) {
    setFlexLayout(true);
    layout().setFlexRows(false);
    layout().setFlexGap(paletteValue(ApplauseModMatrixColumnGap));
    layout().setFlexItemAlignment(visage::Layout::ItemAlignment::Stretch);

    src_menu_.on_build_menu_ = [this](NativePopupMenu& menu) {
        for (uint16_t i = 0; i < owner_.matrix_.getSourceCount(); ++i) {
            menu.addOption(i, owner_.matrix_.getSource(i).name);
            if (i == src_list_id) menu.select(true);
        }
    };

    dst_menu_.on_build_menu_ = [this](NativePopupMenu& menu) {
        for (uint16_t i = 0; i < owner_.matrix_.getDestinationCount(); ++i) {
            menu.addOption(i + 1, owner_.matrix_.getDestination(i).name);
            if (dst_list_id >= 0 && i + 1 == dst_list_id) menu.select(true);
        }

        bool has_connections = false;
        for (auto& conn : owner_.matrix_.getConnections()) {
            if (!conn.isDepthMod()) { has_connections = true; break; }
        }
        if (has_connections) {
            menu.addBreak();
            auto& sub = menu.addSubMenu("Connections");
            for (auto& conn : owner_.matrix_.getConnections()) {
                if (conn.isDepthMod()) continue;
                sub.addOption(-(static_cast<int>(conn.depth_slot) + 1), connectionLabel(owner_.matrix_, conn));
            }
        }
    };

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

        src_menu_.on_item_selected_ += [this](int id) {
            src_list_id = id;
            src_menu_.setText(owner_.matrix_.getSource(id).name);
            if (src_list_id >= 0 && dst_list_id != 0) owner_.activateRow(this);
        };
        dst_menu_.on_item_selected_ += [this](int id) {
            dst_list_id = id;
            if (id < 0) {
                auto slot = static_cast<uint16_t>(-(id + 1));
                if (auto target = owner_.matrix_.findConnection(slot))
                    dst_menu_.setText(connectionLabel(owner_.matrix_, *target));
            } else {
                dst_menu_.setText(owner_.matrix_.getDestination(id - 1).name);
            }
            if (src_list_id >= 0 && dst_list_id != 0) owner_.activateRow(this);
        };
    }
}

void ModMatrixComponent::Row::bindToConnection(const ModConnection& c) {
    is_dummy = false;
    conn = c;
    src_list_id = c.src_idx;

    src_menu_.setText(owner_.matrix_.getSource(c.src_idx).name);

    if (c.isDepthMod()) {
        dst_list_id = -(static_cast<int>(c.dst_idx) + 1);
        if (auto target = owner_.matrix_.findConnection(c.dst_idx))
            dst_menu_.setText(connectionLabel(owner_.matrix_, *target));
        else
            dst_menu_.setText("[deleted]");
        dst_menu_.on_build_menu_ = nullptr;
    } else {
        dst_list_id = c.dst_idx + 1;
        dst_menu_.setText(owner_.matrix_.getDestination(c.dst_idx).name);
    }

    depth_slider_.setValue(c.getDepth());
    bipolar_toggle_.setToggled(c.isBipolar());
    setControlsActive(true);

    depth_slider_.on_value_changed += [this](float value) { conn.setDepth(value); };

    bipolar_toggle_.onToggle() += [this](Button*, bool on) { conn.setBipolar(on); };

    src_menu_.on_item_selected_ += [this](int id) {
        float depth = conn.getDepth();
        bool bipolar = conn.isBipolar();
        owner_.matrix_.removeConnection(conn);

        src_list_id = id;
        src_menu_.setText(owner_.matrix_.getSource(id).name);

        if (conn.isDepthMod()) {
            if (auto target = owner_.matrix_.findConnection(conn.dst_idx))
                conn = owner_.matrix_.addDepthModulation(owner_.matrix_.getSource(id), *target, depth, bipolar);
        } else {
            conn = owner_.matrix_.addConnection(owner_.matrix_.getSource(id),
                                                owner_.matrix_.getDestination(conn.dst_idx), depth, bipolar);
        }
    };

    if (!c.isDepthMod()) {
        dst_menu_.on_item_selected_ += [this](int id) {
            if (id <= 0) return;
            float depth = conn.getDepth();
            bool bipolar = conn.isBipolar();
            owner_.matrix_.removeConnection(conn);

            dst_list_id = id;
            dst_menu_.setText(owner_.matrix_.getDestination(id - 1).name);
            conn = owner_.matrix_.addConnection(owner_.matrix_.getSource(conn.src_idx),
                                                owner_.matrix_.getDestination(id - 1), depth, bipolar);
        };
    }

    delete_button_.onToggle() += [this](Button*, bool) { owner_.deleteRow(this); };
}

void ModMatrixComponent::Row::setControlsActive(bool active) {
    bipolar_toggle_.setActive(active);
    depth_slider_.setActive(active);
    delete_button_.setActive(active);
}

ModMatrixComponent::ModMatrixComponent(applause::ModMatrix& matrix) : matrix_(matrix) {
    scrollableLayout().setFlex(true);
    scrollableLayout().setFlexRows(true);
    scrollableLayout().setFlexGap(paletteValue(ApplauseModMatrixRowGap));
    scrollableLayout().setPaddingLeft(paletteValue(ApplauseModMatrixPadding));
    scrollableLayout().setPaddingRight(paletteValue(ApplauseModMatrixPadding));
    scrollableLayout().setFlexItemAlignment(visage::Layout::ItemAlignment::Stretch);

    setScrollBarRounding(5.0f);

    buildHeader();
    addScrolledChild(&header_);
    rebuildRows();
}

void ModMatrixComponent::buildHeader() {
    header_.setFlexLayout(true);
    header_.layout().setFlexRows(false);
    header_.layout().setFlexGap(paletteValue(ApplauseModMatrixColumnGap));
    header_.layout().setFlexItemAlignment(visage::Layout::ItemAlignment::Stretch);
    header_.layout().setHeight(paletteValue(ApplauseModMatrixRowHeight));
    header_.layout().setFlexGrow(0.0f);
    header_.layout().setFlexShrink(0.0f);

    header_.onDraw() = [&h = header_](visage::Canvas& canvas) {
        canvas.setColor(0xff444444);
        canvas.fill(10, h.height() - 4, h.width() - 20, 1);
    };

    visage::Font header_font(11, applause::fonts::Jost_Regular_ttf);
    auto setupLabel = [&](visage::Frame& frame, const char* text) {
        frame.onDraw() = [&frame, text, header_font](visage::Canvas& canvas) {
            canvas.setColor(0xff888888);
            canvas.text(text, header_font, visage::Font::kCenter, 0, 0, frame.width(), frame.height());
        };
    };

    header_.addChild(&header_source_);
    header_source_.layout().setFlexGrow(1.0f);
    setupLabel(header_source_, "SOURCE");

    header_.addChild(&header_dest_);
    header_dest_.layout().setFlexGrow(1.0f);
    setupLabel(header_dest_, "DESTINATION");

    header_.addChild(&header_polarity_);
    header_polarity_.layout().setWidth(paletteValue(ApplauseModMatrixToggleWidth));
    header_polarity_.layout().setFlexGrow(0.0f);
    header_polarity_.layout().setFlexShrink(0.0f);
    setupLabel(header_polarity_, "POL.");

    header_.addChild(&header_amount_);
    header_amount_.layout().setFlexGrow(1.5f);
    setupLabel(header_amount_, "AMOUNT");

    header_.addChild(&header_delete_);
    header_delete_.layout().setWidth(paletteValue(ApplauseModMatrixDeleteWidth));
    header_delete_.layout().setFlexGrow(0.0f);
    header_delete_.layout().setFlexShrink(0.0f);
}

void ModMatrixComponent::rebuildRows() {
    for (auto& row : rows_) removeScrolledChild(row.get());
    rows_.clear();

    for (auto& conn : matrix_.getConnections()) {
        auto row = std::make_unique<Row>(*this, false);
        row->layout().setHeight(paletteValue(ApplauseModMatrixRowHeight));
        row->layout().setFlexGrow(0.0f);
        row->layout().setFlexShrink(0.0f);
        row->bindToConnection(conn);
        addScrolledChild(row.get());
        rows_.push_back(std::move(row));
    }

    addDummyRow();

    resized();
}

void ModMatrixComponent::resized() {
    ScrollableFrame::resized();

    int bar_width = 10;
    scrollBar().setBounds(width() - bar_width, 0, bar_width, height());

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
    if (row->dst_list_id < 0) {
        auto slot = static_cast<uint16_t>(-(row->dst_list_id + 1));
        auto target = matrix_.findConnection(slot);
        if (!target) return;

        auto conn = matrix_.addDepthModulation(matrix_.getSource(row->src_list_id), *target, 0.0f);
        row->bindToConnection(conn);
    } else {
        auto conn = matrix_.addConnection(matrix_.getSource(row->src_list_id),
                                          matrix_.getDestination(row->dst_list_id - 1), 0.0f);
        row->bindToConnection(conn);
    }
    addDummyRow();

    resized();
}

void ModMatrixComponent::deleteRow(Row* row) {
    if (row->conn.isDepthMod()) {
        matrix_.removeConnection(row->conn);
    } else {
        // Cascade-delete depth mods targeting this connection
        uint16_t target_slot = row->conn.depth_slot;
        std::vector<ModConnection> depth_mods_to_remove;
        for (auto& c : matrix_.getConnections()) {
            if (c.isDepthMod() && c.dst_idx == target_slot)
                depth_mods_to_remove.push_back(c);
        }
        for (auto& dm : depth_mods_to_remove)
            matrix_.removeConnection(dm);

        matrix_.removeConnection(row->conn);
    }

    rebuildRows();
}

}  // namespace applause
