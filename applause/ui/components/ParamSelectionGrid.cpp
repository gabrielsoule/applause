#include "ParamSelectionGrid.h"

namespace applause {

ParamSelectionGrid::ParamSelectionGrid(ParamInfo& parameter, int columns, int rows) :
    grid_(columns, rows), binding_(parameter, grid_.group_) {
    for (int index = 0; index < columns * rows; ++index)
        grid_.emplaceCell<TextSelectionGridCell>(
            index, parameter.valueToText(static_cast<float>(static_cast<double>(parameter.minValue) + index)));

    grid_.onSelectionChanged() += [this](int index) { on_selection_changed_.callback(index); };

    addChild(&grid_);
}

void ParamSelectionGrid::resized() { grid_.setBounds(localBounds()); }

}  // namespace applause
