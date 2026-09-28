#pragma once

#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/components/Checkbox.h>
#include <applause/util/thirdparty/rocket.hpp>

namespace applause {

/// A checkbox attached to a stepped parameter with exactly two adjacent integer values.
/// Off maps to minValue and on maps to maxValue.
class ParamCheckbox : public Checkbox {
public:
    explicit ParamCheckbox(ParamInfo& parameter, std::string label = {});

protected:
    void toggleStateChanged(bool notify) override;
    bool allowsRadioGroup() const override { return false; }

private:
    void updateFromParameter(float value);

    ParamInfo& parameter_;
    rocket::scoped_connection parameter_connection_;
};

}  // namespace applause
