#pragma once

#include <applause/ui/components/Button.h>

namespace applause {

/// A square toggle with an optional label. 
class Checkbox : public ToggleButton {
public:
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseCheckboxSize);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseCheckboxTextGap);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseCheckboxBorderWidth);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseCheckboxRounding);

    explicit Checkbox(std::string text = {});

    const applause::String& text() const noexcept { return text_.text(); }
    void setText(std::string text);
    void setFont(const applause::Font& font);

    void draw(applause::Canvas& canvas) override;

private:
    applause::Text text_;
};

}  // namespace applause
