#pragma once

#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/components/Dropdown.h>
#include <applause/util/thirdparty/rocket.hpp>

namespace applause {

// A dropdown attached to a stepped parameter with whole-number bounds, with one option for each value; an option's ID
// is its value minus minValue. The options are the parameter's value texts, so an enumerated parameter shows its
// choices. A pick sends one host gesture.
class ParamDropdown : public Dropdown {
public:
    explicit ParamDropdown(ParamInfo& parameter);

protected:
    void selectionChanged(bool notify) override;

private:
    void updateFromParameter(float value);

    ParamInfo& parameter_;
    rocket::scoped_connection parameter_connection_;
};

}  // namespace applause
