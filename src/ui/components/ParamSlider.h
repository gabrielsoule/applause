#pragma once

#include <visage_ui/frame.h>
#include <visage_widgets/text_editor.h>
#include <visage_graphics/palette.h>
#include "util/thirdparty/rocket.hpp"
#include "Slider.h"
#include <memory>
#include <extensions/ParamsExtension.h>

namespace applause {

/**
 * This component wraps a Slider and a text box together. It also
 * connects to parameter changes via rocket signal system.
 */
class ParamSlider : public visage::Frame
{
public:
    ParamSlider(ParamInfo& paramInfo);

    void resized() override;
    void draw(visage::Canvas& canvas) override;

private:
    void updateTextDisplay();
    
    static constexpr int kLabelWidth = 80;
    static constexpr int kLabelPadding = 10;

    ParamInfo& param_info_;
    std::unique_ptr<Slider> slider_;
    visage::TextEditor* text_editor_ = nullptr;  // Raw pointer, owned by parent
    rocket::scoped_connection param_connection_;

    // Text box handling
    float original_value_ = 0.0f;
    bool is_editing_ = false;
};

} // namespace applause
