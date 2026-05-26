#pragma once

#include <applause/ui/ApplauseUI.h>

#include <applause/core/ModMatrix.h>
#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/components/Knob.h>
#include <applause/ui/components/ParamValueTextBox.h>
#include <applause/util/thirdparty/rocket.hpp>

namespace applause {

/**
 * A component that wraps a Knob with parameter connection and label display.
 * Displays the parameter's shortName below the knob.
 */
class ParamKnob : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseParamKnobText);

    ParamKnob(ParamInfo& paramInfo, const ModDestination* dst = nullptr);

    void draw(applause::Canvas& canvas) override;
    void resized() override;

    void mouseEnter(const applause::MouseEvent& e) override;
    void mouseExit(const applause::MouseEvent& e) override;

private:
    static constexpr float kLabelHeight = 20.0f;
    static constexpr float kLabelPadding = 2.0f;

    ParamInfo& param_info_;
    Knob knob_;
    ParamValueTextBox paramValueText_;
    applause::TextEditor paramNameText_{"param_name"};
    applause::Palette name_text_palette_;
    rocket::scoped_connection param_connection_;
    rocket::scoped_connection mod_changed_conn_;
    const ModDestination* destination_ = nullptr;
    bool mouseOver_ = false;
};

}  // namespace applause
