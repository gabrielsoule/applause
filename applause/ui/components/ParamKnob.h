#pragma once

#include <visage_ui/frame.h>
#include <visage_widgets/text_editor.h>

#include <memory>

#include "Knob.h"
#include "ParamValueTextBox.h"
#include "applause/extensions/ParamsExtension.h"
#include "applause/util/thirdparty/rocket.hpp"

namespace applause {

/**
 * A component that wraps a Knob with parameter connection and label display.
 * Displays the parameter's shortName below the knob.
 */
class ParamKnob : public visage::Frame {
public:
    ParamKnob(ParamInfo& paramInfo);

    void draw(visage::Canvas& canvas) override;
    void resized() override;

    void mouseEnter(const visage::MouseEvent& e) override;
    void mouseExit(const visage::MouseEvent& e) override;

private:
    static constexpr float kLabelHeight = 20.0f;
    static constexpr float kLabelPadding = 2.0f;

    ParamInfo& param_info_;
    std::unique_ptr<ParamValueTextBox> paramValueText_;
    std::unique_ptr<visage::TextEditor> paramNameText_;
    visage::Palette name_text_palette_;
    std::unique_ptr<Knob> knob_;
    rocket::scoped_connection param_connection_;
    bool mouseOver_ = false;
};

}  // namespace applause
