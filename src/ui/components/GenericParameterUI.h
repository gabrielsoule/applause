#pragma once

#include <visage_ui/frame.h>
#include <visage_ui/scroll_bar.h>
#include "ParamSlider.h"
#include <vector>
#include <memory>

namespace applause {

/**
 * A Frame that wraps a ParamSlider while placing
 * label text, with the name of the parameter, to the left of the ParamSlider.
 * The GenericParameterEntry reserves a fixed number of pixels for the label,
 * and adjusts the ParamSlider to fit the rest of the horizontal space.
 * The ParamSlider inherits the full height of the GenericParameterEntry.
 */
class GenericParameterEntry : public visage::Frame {
public:
    GenericParameterEntry(ParamInfo& paramInfo);

    void draw(visage::Canvas& canvas) override;
    void resized() override;
    
    void setLabelWidth(float labelWidth);

private:
    static constexpr int kLabelPadding = 10;

    ParamInfo& paramInfo_;
    std::unique_ptr<ParamSlider> paramSlider_;
    float labelWidth_ = 100.0;
};


class GenericParameterUI : public visage::ScrollableFrame {

public:
    GenericParameterUI();
    void draw(visage::Canvas& canvas) override;
    void resized() override;
    void addParameter(ParamInfo& paramInfo);

private:
    static constexpr float kPadding = 16.0f;
    static constexpr float kEntryGap = 16.0f;
    static constexpr float kEntryHeight = 26.0f;

    std::vector<std::unique_ptr<GenericParameterEntry>> entries_;
};

} // namespace applause
