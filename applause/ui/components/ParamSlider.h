#pragma once

#include <applause/ui/ApplauseUI.h>

#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/components/ParamValueTextBox.h>
#include <applause/ui/components/Slider.h>
#include <applause/util/thirdparty/rocket.hpp>

namespace applause {

/**
 * This component wraps a Slider and a ParamValueTextBox together. It also
 * connects to parameter changes via rocket signal system.
 */
class ParamSlider : public applause::Frame {
public:
    ParamSlider(ParamInfo& paramInfo);

    void resized() override;
    void draw(applause::Canvas& canvas) override;

private:
    static constexpr int kLabelWidth = 80;
    static constexpr int kLabelPadding = 5;

    ParamInfo& param_info_;
    Slider slider_;
    ParamValueTextBox param_text_box_;
    rocket::scoped_connection param_connection_;
};

}  // namespace applause
