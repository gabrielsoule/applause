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

    ParamKnob(ParamInfo& paramInfo);

    void draw(applause::Canvas& canvas) override;
    void resized() override;

    void mouseEnter(const applause::MouseEvent& e) override;
    void mouseExit(const applause::MouseEvent& e) override;

    void setModDestination(ModMatrix* matrix, const ModDestination* dst);

private:
    static constexpr float kLabelHeight = 20.0f;
    static constexpr float kLabelPadding = 2.0f;

    class ModOverlay : public applause::Frame {
    public:
        explicit ModOverlay(const ParamKnob& owner);
        void draw(applause::Canvas& canvas) override;

    private:
        const ParamKnob* owner_;
    };

    void refreshConnectionState();

    ParamInfo& param_info_;
    Knob knob_;
    ModOverlay mod_overlay_;
    ParamValueTextBox paramValueText_;
    applause::TextEditor paramNameText_{"param_name"};
    applause::Palette name_text_palette_;
    rocket::scoped_connection param_connection_;
    ModMatrix* mod_matrix_ = nullptr;
    const ModDestination* destination_ = nullptr;
    bool is_connected_ = false;
    rocket::scoped_connection connections_changed_conn_;
    bool mouseOver_ = false;
};

}  // namespace applause
