#pragma once

#include <applause/ui/ApplauseUI.h>

#include <applause/core/ModMatrix.h>

#include <applause/ui/components/Button.h>
#include <applause/ui/components/Slider.h>

#include <memory>
#include <string>
#include <vector>

namespace applause {

class ModMatrixComponent : public applause::ScrollableFrame {
public:
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseModMatrixRowHeight);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseModMatrixRowGap);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseModMatrixPadding);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseModMatrixColumnGap);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseModMatrixToggleWidth);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseModMatrixDeleteWidth);

    /**
     * A single row in the mod matrix UI, representing one modulation connection.
     * Can be a "dummy" row (awaiting user input) or an active row bound to a connection.
     */
    class Row : public applause::Frame {
    public:
        Row(ModMatrixComponent& owner, bool is_dummy);

        void bindToConnection(const ModConnection& conn);

        bool is_dummy;
        int src_list_id = -1;  // ModMatrix source index, or -1 if unset
        int dst_list_id = 0;  // 0 = unset, positive = ModMatrix dest index + 1, negative = -(depth_slot + 1)
        ModConnection conn;  // Copied from matrix on bind; only valid when !is_dummy

    private:
        void setControlsActive(bool active);

        ModMatrixComponent& owner_;

        applause::PopupMenuButton src_menu_{"—"};
        applause::PopupMenuButton dst_menu_{"—"};
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
    void buildHeader();

    applause::ModMatrix& matrix_;
    applause::Frame header_;
    applause::Frame header_source_;
    applause::Frame header_dest_;
    applause::Frame header_polarity_;
    applause::Frame header_amount_;
    applause::Frame header_delete_;
    std::vector<std::unique_ptr<Row>> rows_;
};

}  // namespace applause
