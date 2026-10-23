#pragma once

#include <applause/ui/components/Button.h>

namespace applause {

/// A circular toggle with an optional label. Add it to a RadioGroup for exclusive selection.
class RadioButton : public ToggleButton {
public:
    static inline const ValueToken kSize{"radioButton.size", 16.0f};
    static inline const ValueToken kTextGap{"radioButton.textGap", 8.0f};
    static inline const ValueToken kBorderWidth{"radioButton.borderWidth", 1.5f};

    explicit RadioButton(std::string text = {});

    const applause::String& text() const noexcept { return text_.text(); }
    void setText(std::string text);
    void setFont(const applause::Font& font);

    void draw(applause::Canvas& canvas) override;

private:
    applause::Text text_;
};

}  // namespace applause
