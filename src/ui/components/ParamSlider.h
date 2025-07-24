#pragma once

#include <visage_ui/frame.h>
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

    void draw(visage::Canvas& canvas) override;
    void resized() override;

private:
    static constexpr int kLabelWidth = 80;
    static constexpr int kLabelPadding = 5;

    ParamInfo& param_info_;
    std::unique_ptr<Slider> slider_;
    rocket::scoped_connection param_connection_;
};

} // namespace applause
