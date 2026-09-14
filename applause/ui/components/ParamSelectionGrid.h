#pragma once

#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/components/SelectionGrid.h>
#include <applause/util/thirdparty/rocket.hpp>

#include <concepts>
#include <utility>

namespace applause {

/// A two dimensional grid of selectable cells, attached to a parameter.
class ParamSelectionGrid : public applause::Frame {
public:
    /// Connects a grid to a stepped parameter with whole-number bounds.
    /// Selecting a cell updates the parameter and sends a host gesture.
    ParamSelectionGrid(ParamInfo& parameter, int columns, int rows);

    /// Returns the selected cell index, starting at 0, rather than the parameter value.
    [[nodiscard]] int selectedIndex() const noexcept { return grid_.selectedIndex(); }

    /// Called with the cell index after a selection updates the parameter.
    /// Note: Changes coming from the parameter update the grid without calling this.
    auto& onSelectionChanged() { return on_selection_changed_; }

    SelectionGridCell& cell(int index) { return grid_.cell(index); }
    const SelectionGridCell& cell(int index) const { return grid_.cell(index); }

    /// Replaces a cell with your own SelectionGridCell subclass and returns a reference to it.
    /// The grid owns the new cell. Call this before the grid is initialized.
    template <std::derived_from<SelectionGridCell> T, typename... Args>
    T& emplaceCell(int index, Args&&... args) {
        return grid_.emplaceCell<T>(index, std::forward<Args>(args)...);
    }

    void resized() override;

private:
    ParamInfo& parameter_;
    SelectionGrid grid_;
    rocket::scoped_connection parameter_connection_;
    applause::CallbackList<void(int)> on_selection_changed_;
};

}  // namespace applause
