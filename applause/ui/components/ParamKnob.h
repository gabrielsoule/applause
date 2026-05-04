#pragma once

#include <visage_graphics/theme.h>
#include <visage_ui/frame.h>
#include <visage_widgets/text_editor.h>

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
class ParamKnob : public visage::Frame {
public:
    VISAGE_THEME_DEFINE_COLOR(ApplauseParamKnobText);

    ParamKnob(ParamInfo& paramInfo);

    void draw(visage::Canvas& canvas) override;
    void resized() override;

    void mouseEnter(const visage::MouseEvent& e) override;
    void mouseExit(const visage::MouseEvent& e) override;

    void setModDestination(ModMatrix* matrix, const ModDestination* dst);

private:
    static constexpr float kLabelHeight = 20.0f;
    static constexpr float kLabelPadding = 2.0f;

    class ModOverlay : public visage::Frame {
    public:
        explicit ModOverlay(const ParamKnob& owner);
        void draw(visage::Canvas& canvas) override;

    private:
        const ParamKnob* owner_;
    };

    void refreshConnectionState();

    ParamInfo& param_info_;
    Knob knob_;
    ModOverlay mod_overlay_;
    ParamValueTextBox paramValueText_;
    visage::TextEditor paramNameText_{"param_name"};
    visage::Palette name_text_palette_;
    rocket::scoped_connection param_connection_;
    ModMatrix* mod_matrix_ = nullptr;
    const ModDestination* destination_ = nullptr;
    bool is_connected_ = false;
    rocket::scoped_connection connections_changed_conn_;
    bool mouseOver_ = false;
};

}  // namespace applause
