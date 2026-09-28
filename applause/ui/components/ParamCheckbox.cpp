#include "ParamCheckbox.h"

#include <cmath>
#include <stdexcept>

namespace applause {

ParamCheckbox::ParamCheckbox(ParamInfo& parameter, std::string label) :
    Checkbox(label.empty() ? (parameter.shortName.empty() ? parameter.name : parameter.shortName) : label),
    parameter_(parameter) {
    if (!parameter_.stepped) throw std::invalid_argument("ParamCheckbox requires a stepped parameter");
    if (!std::isfinite(parameter_.minValue) || !std::isfinite(parameter_.maxValue) ||
        std::trunc(parameter_.minValue) != parameter_.minValue ||
        std::trunc(parameter_.maxValue) != parameter_.maxValue ||
        static_cast<double>(parameter_.maxValue) - static_cast<double>(parameter_.minValue) != 1.0)
        throw std::invalid_argument("ParamCheckbox requires two adjacent integer parameter values");

    updateFromParameter(parameter_.getValue());
    parameter_connection_ = parameter_.on_value_changed.connect([this](float value) { updateFromParameter(value); });
}

void ParamCheckbox::toggleStateChanged(bool notify) {
    if (!notify) return;

    const float value = toggled() ? parameter_.maxValue : parameter_.minValue;
    parameter_.beginGesture();
    parameter_.setValueNotifyingHost(value);
    parameter_.endGesture();
}

void ParamCheckbox::updateFromParameter(float value) {
    if (value == parameter_.minValue || value == parameter_.maxValue) setToggled(value == parameter_.maxValue);
}

}  // namespace applause
