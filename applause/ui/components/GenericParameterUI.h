#pragma once

#include <applause/ui/ApplauseUI.h>

#include <memory>
#include <optional>
#include <vector>

#include <applause/ui/components/ParamSlider.h>

namespace applause {
/**
 * A Frame that wraps a ParamSlider while placing
 * label text, with the name of the parameter, to the left of the ParamSlider.
 * The GenericParameterEntry reserves a theme-able width for the label,
 * and adjusts the ParamSlider to fit the rest of the horizontal space.
 * The ParamSlider inherits the full height of the GenericParameterEntry.
 */
class GenericParameterEntry : public applause::Frame {
public:
    static inline const ValueToken kLabelWidth{"genericParameterEntry.labelWidth", 100.0f};
    static inline const ValueToken kLabelPadding{"genericParameterEntry.labelPadding", 10.0f};

    GenericParameterEntry(ParamInfo& paramInfo);

    void draw(applause::Canvas& canvas) override;

    void resized() override;

    void setLabelWidth(float labelWidth);
    void setFont(const applause::Font& font);

private:
    ParamInfo& paramInfo_;
    ParamSlider paramSlider_;
    applause::Font font_;
};

class GenericParameterUI : public applause::ScrollableFrame {
public:
    static inline const ValueToken kPadding{"genericParameterUI.padding", 16.0f};
    static inline const ValueToken kEntryGap{"genericParameterUI.entryGap", 16.0f};
    static inline const ValueToken kEntryHeight{"genericParameterUI.entryHeight", 26.0f};

    GenericParameterUI();

    void draw(applause::Canvas& canvas) override;

    void resized() override;

    void addParameter(ParamInfo& paramInfo);
    void setFont(const applause::Font& font);

private:
    std::vector<std::unique_ptr<GenericParameterEntry>> entries_;
    std::optional<applause::Font> font_override_;
};
}  // namespace applause
