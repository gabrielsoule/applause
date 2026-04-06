#pragma once

#include <applause/core/ModMatrix.h>
#include <visage_graphics/theme.h>
#include <visage_ui/scroll_bar.h>

#include "ApplauseButton.h"
#include "Slider.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace applause {

class ModMatrixComponent : public visage::ScrollableFrame {
public:
    VISAGE_THEME_DEFINE_VALUE(ApplauseModMatrixRowHeight);
    VISAGE_THEME_DEFINE_VALUE(ApplauseModMatrixRowGap);
    VISAGE_THEME_DEFINE_VALUE(ApplauseModMatrixPadding);
    VISAGE_THEME_DEFINE_VALUE(ApplauseModMatrixColumnGap);
    VISAGE_THEME_DEFINE_VALUE(ApplauseModMatrixToggleWidth);
    VISAGE_THEME_DEFINE_VALUE(ApplauseModMatrixDeleteWidth);

    /**
     * Button that shows a native popup menu on click and displays the selected value.
     * Shows a default placeholder ("--") when nothing is selected.
     */
    class PopupMenuComponent : public applause::UiButton {
    public:
        explicit PopupMenuComponent(const std::string& default_text = "--");

        void setItems(std::vector<std::pair<int, std::string>> items);
        void showPopup();
        void setSelectedId(int id);
        void reset();

        [[nodiscard]] int selectedId() const { return selected_id_; }
        [[nodiscard]] const std::string& selectedName() const { return selected_name_; }

        visage::CallbackList<void(int)> on_item_selected_;

    private:
        std::string default_text_;
        std::string selected_name_;
        int selected_id_ = -1;
        std::vector<std::pair<int, std::string>> items_;
    };

    /**
     * A single row in the mod matrix UI, representing one modulation connection.
     * Can be a "dummy" row (awaiting user input) or an active row bound to a connection.
     */
    class Row : public visage::Frame {
    public:
        Row(ModMatrixComponent& owner, bool is_dummy);

        void bindToConnection(const ModConnection& conn);
        [[nodiscard]] bool isDummy() const { return is_dummy_; }
        [[nodiscard]] int srcIdx() const { return src_idx_; }
        [[nodiscard]] int dstIdx() const { return dst_idx_; }

    private:
        void setControlsActive(bool active);

        ModMatrixComponent& owner_;
        bool is_dummy_;
        int src_idx_ = -1;
        int dst_idx_ = -1;

        PopupMenuComponent src_menu_{"—"};
        PopupMenuComponent dst_menu_{"—"};
        applause::ToggleTextButton bipolar_toggle_{"±"};
        applause::Slider depth_slider_;
        applause::UiButton delete_button_{"X"};
    };

    explicit ModMatrixComponent(applause::ModMatrix& matrix);

    void rebuildRows();
    void resized() override;

private:
    void addDummyRow();
    void activateRow(Row* row);
    void deleteRow(Row* row);

    applause::ModMatrix& matrix_;
    std::vector<std::unique_ptr<Row>> rows_;
};

}  // namespace applause
