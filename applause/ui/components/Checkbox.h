#pragma once

#include <applause/ui/components/Button.h>

namespace applause {

/// A square toggle with an optional label. 
class Checkbox : public ToggleButton {
public:
    static inline const ValueToken kSize{"checkbox.size", 16.0f};
    static inline const ValueToken kTextGap{"checkbox.textGap", 8.0f};
    static inline const ValueToken kBorderWidth{"checkbox.borderWidth", 1.5f};
    static inline const ValueToken kRounding{"checkbox.rounding", 3.0f};

    explicit Checkbox(std::string text = {});

    const applause::String& text() const noexcept { return text_.text(); }
    void setText(std::string text);
    void setFont(const applause::Font& font);

    void draw(applause::Canvas& canvas) override;

private:
    applause::Text text_;
};

}  // namespace applause
