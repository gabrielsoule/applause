#include "ParamSelectionGrid.h"

#include <cmath>
#include <stdexcept>

namespace applause {

ParamSelectionGrid::ParamSelectionGrid(ParamInfo& parameter, int columns, int rows) :
    parameter_(parameter), grid_(columns, rows) {
    if (!parameter_.stepped) throw std::invalid_argument("ParamSelectionGrid requires a stepped parameter");
    if (!std::isfinite(parameter_.minValue) || !std::isfinite(parameter_.maxValue) ||
        std::trunc(parameter_.minValue) != parameter_.minValue ||
        std::trunc(parameter_.maxValue) != parameter_.maxValue)
        throw std::invalid_argument("ParamSelectionGrid requires integral parameter bounds");

    const auto grid_cells = static_cast<long long>(columns) * rows;
    const double parameter_values =
        static_cast<double>(parameter_.maxValue) - static_cast<double>(parameter_.minValue) + 1.0;
    if (parameter_values != static_cast<double>(grid_cells))
        throw std::invalid_argument("ParamSelectionGrid cell count must match the parameter range");

    for (int index = 0; index < grid_cells; ++index)
        grid_.emplaceCell<TextSelectionGridCell>(
            index, parameter_.valueToText(parameter_.minValue + static_cast<float>(index)));

    const int initial_index = static_cast<int>(parameter_.getValue() - parameter_.minValue);
    grid_.setSelectedIndex(initial_index);

    grid_.onSelectionChanged() += [this](int index) {
        parameter_.beginGesture();
        parameter_.setValueNotifyingHost(parameter_.minValue + static_cast<float>(index));
        parameter_.endGesture();
        on_selection_changed_.callback(index);
    };

    parameter_connection_ = parameter_.on_value_changed.connect(
        [this](float value) { grid_.setSelectedIndex(static_cast<int>(value - parameter_.minValue)); });

    addChild(&grid_);
}

void ParamSelectionGrid::resized() { grid_.setBounds(localBounds()); }

}  // namespace applause
