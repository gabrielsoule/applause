#pragma once

#include <applause/ui/ApplauseUI.h>

// visage palette depends on gradient/theme definitions; keep include order.
// clang-format off
// clang-format on

#include <applause/extensions/ParamsExtension.h>
#include <applause/util/thirdparty/rocket.hpp>

namespace applause {

/**
 * A text box component that displays and allows editing of a parameter value.
 * Features adaptive precision formatting and validates input using
 * ParamInfo::parseText.
 */
class ParamValueTextBox : public applause::Frame {
public:
    explicit ParamValueTextBox(ParamInfo& paramInfo);

    void resized() override;

    /** Force update the displayed text to match the current parameter value */
    void updateTextDisplay();

private:
    ParamInfo& param_info_;
    applause::TextEditor text_editor_{"param_value"};
    rocket::scoped_connection param_connection_;

    // Edit state tracking
    float original_value_ = 0.0f;
    bool is_editing_ = false;
};

}  // namespace applause
