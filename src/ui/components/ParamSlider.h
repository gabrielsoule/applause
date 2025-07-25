#pragma once

#include <visage_ui/frame.h>
#include "util/thirdparty/rocket.hpp"
#include "Slider.h"
#include "ParamValueTextBox.h"
#include <memory>
#include <extensions/ParamsExtension.h>

namespace applause {

/**
 * This component wraps a Slider and a ParamValueTextBox together. It also
 * connects to parameter changes via rocket signal system.
 */
class ParamSlider : public visage::Frame
{
public:
    ParamSlider(ParamInfo& paramInfo);

    void resized() override;
    void draw(visage::Canvas& canvas) override;

private:
    static constexpr int kLabelWidth = 80;
    static constexpr int kLabelPadding = 10;

    ParamInfo& param_info_;
    std::unique_ptr<Slider> slider_;
    std::unique_ptr<ParamValueTextBox> param_text_box_;
    rocket::scoped_connection param_connection_;
};

} // namespace applause
