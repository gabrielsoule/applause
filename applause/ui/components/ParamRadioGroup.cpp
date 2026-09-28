#include "ParamRadioGroup.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace applause {

ParamRadioGroup::ParamRadioGroup(ParamInfo& parameter, RadioGroup& group) { attach(parameter, group); }

void ParamRadioGroup::attach(ParamInfo& parameter, RadioGroup& group) {
    if (group_) throw std::logic_error("ParamRadioGroup is already attached");
    if (group.binding_invalidated_) throw std::logic_error("RadioGroup already has a parameter binding");
    if (!parameter.stepped) throw std::invalid_argument("ParamRadioGroup requires a stepped parameter");
    if (!std::isfinite(parameter.minValue) || !std::isfinite(parameter.maxValue) ||
        std::trunc(parameter.minValue) != parameter.minValue || std::trunc(parameter.maxValue) != parameter.maxValue)
        throw std::invalid_argument("ParamRadioGroup requires integral parameter bounds");

    const double value_count = static_cast<double>(parameter.maxValue) - parameter.minValue + 1.0;
    if (value_count != static_cast<double>(group.members_.size()) || group.members_.empty() ||
        group.members_.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw std::invalid_argument("ParamRadioGroup member count must match the parameter range");
    constexpr double max_exact_integer = 1u << std::numeric_limits<float>::digits;
    if (value_count > 1.0 &&
        (parameter.minValue < -max_exact_integer || parameter.maxValue > max_exact_integer))
        throw std::invalid_argument("ParamRadioGroup values must be representable as consecutive floats");

    for (const auto& member : group.members_) {
        if (member.id < 0 || static_cast<std::size_t>(member.id) >= group.members_.size())
            throw std::invalid_argument("ParamRadioGroup IDs must cover the parameter range starting at zero");
    }

    parameter_ = &parameter;
    group_ = &group;
    try {
        parameter_connection_ = parameter.on_value_changed.connect([this](float value) { updateFromParameter(value); });
        group.parameter_changed_ = [parameter = &parameter](int id) {
            parameter->beginGesture();
            parameter->setValueNotifyingHost(static_cast<float>(static_cast<double>(parameter->minValue) + id));
            parameter->endGesture();
        };
        group.binding_invalidated_ = [this] { detach(); };
        updateFromParameter(parameter.getValue());
    } catch (...) {
        detach();
        throw;
    }
}

ParamRadioGroup::~ParamRadioGroup() { detach(); }

void ParamRadioGroup::detach() noexcept {
    parameter_connection_.disconnect();
    if (group_) {
        group_->parameter_changed_ = nullptr;
        group_->binding_invalidated_ = nullptr;
        group_ = nullptr;
    }
    parameter_ = nullptr;
}

void ParamRadioGroup::updateFromParameter(float value) {
    if (!group_ || !std::isfinite(value) || std::trunc(value) != value || value < parameter_->minValue ||
        value > parameter_->maxValue)
        return;

    const double id = static_cast<double>(value) - parameter_->minValue;
    if (id < 0.0 || id >= static_cast<double>(group_->members_.size())) return;
    group_->setSelectedId(static_cast<int>(id));
}

}  // namespace applause
