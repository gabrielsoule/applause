#pragma once

#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/components/RadioGroup.h>
#include <applause/util/thirdparty/rocket.hpp>

namespace applause {

/// Binds an assembled radio group to a stepped parameter. An existing ParamRadioGroup should not be re-bound after
/// being bound once.
/// IDs start at zero, with one member for each integer in the parameter range.
class ParamRadioGroup {
public:
    /// Creates an unattached binding. You MUST call attach() after assembling the group!
    ParamRadioGroup() = default;
    ParamRadioGroup(ParamInfo& parameter, RadioGroup& group);
    ~ParamRadioGroup();

    ParamRadioGroup(const ParamRadioGroup&) = delete;
    ParamRadioGroup& operator=(const ParamRadioGroup&) = delete;
    ParamRadioGroup(ParamRadioGroup&&) = delete;
    ParamRadioGroup& operator=(ParamRadioGroup&&) = delete;

    /// Attaches and initializes the group silently.
    /// Throws std::logic_error if this object or group is already attached.
    void attach(ParamInfo& parameter, RadioGroup& group);

private:
    void detach() noexcept;
    void updateFromParameter(float value);

    ParamInfo* parameter_ = nullptr;
    RadioGroup* group_ = nullptr;
    rocket::scoped_connection parameter_connection_;
};

}  // namespace applause
