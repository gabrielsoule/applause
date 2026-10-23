#pragma once

#include <applause/ui/components/Button.h>

namespace applause {

/// A circular toggle with an optional label. Add it to a RadioGroup for exclusive selection.
class RadioButton : public Button {
public:
    static inline const ValueToken kSize{"radioButton.size", 16.0f};
    static inline const ValueToken kTextGap{"radioButton.textGap", 8.0f};
    static inline const ValueToken kBorderWidth{"radioButton.borderWidth", 1.5f};

    explicit RadioButton(std::string text = {});

protected:
    void drawBackground(applause::Canvas& canvas, float hover) override {}
    void drawContent(applause::Canvas& canvas, float hover) override;
};

}  // namespace applause
