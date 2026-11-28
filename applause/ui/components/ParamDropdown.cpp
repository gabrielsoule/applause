#include "ParamDropdown.h"

#include <cmath>
#include <stdexcept>

namespace applause {

ParamDropdown::ParamDropdown(ParamInfo& parameter) : parameter_(parameter) {
    if (!parameter_.stepped) throw std::invalid_argument("ParamDropdown requires a stepped parameter");
    if (!std::isfinite(parameter_.minValue) || !std::isfinite(parameter_.maxValue) ||
        std::trunc(parameter_.minValue) != parameter_.minValue ||
        std::trunc(parameter_.maxValue) != parameter_.maxValue)
        throw std::invalid_argument("ParamDropdown requires integral parameter bounds");

    std::vector<std::string> options;
    for (double value = parameter_.minValue; value <= parameter_.maxValue; ++value)
        options.push_back(parameter_.valueToText(static_cast<float>(value)));
    setOptions(options);

    updateFromParameter(parameter_.getValue());
    parameter_connection_ = parameter_.on_value_changed.connect([this](float value) { updateFromParameter(value); });
}

void ParamDropdown::selectionChanged(bool notify) {
    if (!notify || !selectedId()) return;

    parameter_.beginGesture();
    parameter_.setValueNotifyingHost(static_cast<float>(static_cast<double>(parameter_.minValue) + *selectedId()));
    parameter_.endGesture();
}

void ParamDropdown::updateFromParameter(float value) {
    if (!std::isfinite(value) || value < parameter_.minValue || value > parameter_.maxValue) return;
    setSelectedId(static_cast<int>(std::trunc(value) - parameter_.minValue));
}

}  // namespace applause
