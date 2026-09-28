#include "Checkbox.h"

#include "RadioButton.h"

#include <embedded/applause_fonts.h>

#include <algorithm>

namespace applause {

namespace {

void drawToggleIndicator(ToggleButton& button, applause::Canvas& canvas, applause::Text& text, float hover,
                         float indicator_size, float text_gap, float border_width, float rounding, bool radio) {
    const float size = std::max(0.0f, std::min({indicator_size, button.width(), button.height()}));
    const float y = (button.height() - size) * 0.5f;
    const float border = std::clamp(border_width, 0.0f, size * 0.5f);
    rounding = std::clamp(rounding, 0.0f, size * 0.5f);

    if (size > 0.0f) {
        canvas.setBlendedColor(Button::ApplauseButtonBackgroundTop, Button::ApplauseButtonBackgroundTopHover, hover);
        if (radio)
            canvas.circle(0.0f, y, size);
        else
            canvas.roundedRectangle(0.0f, y, size, size, rounding);

        const float glow = button.isActive()
                               ? std::min(1.0f, 1.75f * hover * canvas.value(Button::ApplauseButtonGlowAmount))
                               : 0.0f;
        if (glow > 0.0f) {
            const auto accent = canvas.color(Button::ApplauseButtonGlow).gradient().sample(0.0f);
            const applause::Point center{size * 0.5f, y + size * 0.5f};
            canvas.setColor(applause::Brush::radial(accent.withAlpha(glow), applause::Color(0x00000000), center,
                                                  size * 0.65f));
            if (radio)
                canvas.circle(0.0f, y, size);
            else
                canvas.roundedRectangle(0.0f, y, size, size, rounding);
        }

        if (!button.isActive())
            canvas.setColor(ToggleButton::ApplauseToggleButtonDisabled);
        else if (button.isPressed())
            canvas.setColor(Button::ApplauseButtonTextPressed);
        else if (button.toggled())
            canvas.setBlendedColor(ToggleButton::ApplauseToggleButtonOn, ToggleButton::ApplauseToggleButtonOnHover,
                                   hover);
        else
            canvas.setBlendedColor(ToggleButton::ApplauseToggleButtonOff, ToggleButton::ApplauseToggleButtonOffHover,
                                   hover);

        if (radio)
            canvas.ring(0.0f, y, size, border);
        else
            canvas.roundedRectangleBorder(0.0f, y, size, size, rounding, border);

        if (button.toggled()) {
            if (radio) {
                const float dot_size = size * 0.45f;
                const float inset = (size - dot_size) * 0.5f;
                canvas.circle(inset, y + inset, dot_size);
            } else {
                const float thickness = size * 0.12f;
                canvas.segment(size * 0.23f, y + size * 0.51f, size * 0.43f, y + size * 0.7f, thickness, true);
                canvas.segment(size * 0.43f, y + size * 0.7f, size * 0.78f, y + size * 0.29f, thickness, true);
            }
        }
    }

    if (!button.isActive())
        canvas.setColor(ToggleButton::ApplauseToggleButtonDisabled);
    else if (button.isPressed())
        canvas.setColor(Button::ApplauseButtonTextPressed);
    else
        canvas.setBlendedColor(Button::ApplauseButtonText, Button::ApplauseButtonTextHover, hover);

    const float text_x = size + std::max(0.0f, text_gap);
    const float text_width = std::max(0.0f, button.width() - text_x);
    if (!text.text().isEmpty() && text_width > 0.0f)
        canvas.text(&text, text_x, 0.0f, text_width, button.height());
}

}  // namespace

APPLAUSE_THEME_IMPLEMENT_VALUE(Checkbox, ApplauseCheckboxSize, 16.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Checkbox, ApplauseCheckboxTextGap, 8.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Checkbox, ApplauseCheckboxBorderWidth, 1.5f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Checkbox, ApplauseCheckboxRounding, 3.0f);

Checkbox::Checkbox(std::string text) :
    ToggleButton(text), text_(text, applause::Font(12, applause::fonts::Barlow_Medium_ttf), applause::Font::kLeft) {}

void Checkbox::setText(std::string text) {
    text_.setText(text);
    redraw();
}

void Checkbox::setFont(const applause::Font& font) {
    text_.setFont(font);
    redraw();
}

void Checkbox::draw(applause::Canvas& canvas) {
    const float hover = isActive() ? hover_amount_.update() : 0.0f;
    drawToggleIndicator(*this, canvas, text_, hover, canvas.value(ApplauseCheckboxSize),
                        canvas.value(ApplauseCheckboxTextGap), canvas.value(ApplauseCheckboxBorderWidth),
                        canvas.value(ApplauseCheckboxRounding), false);
    if (isActive() && hover_amount_.isAnimating()) redraw();
}

APPLAUSE_THEME_IMPLEMENT_VALUE(RadioButton, ApplauseRadioButtonSize, 16.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(RadioButton, ApplauseRadioButtonTextGap, 8.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(RadioButton, ApplauseRadioButtonBorderWidth, 1.5f);

RadioButton::RadioButton(std::string text) :
    ToggleButton(text), text_(text, applause::Font(12, applause::fonts::Barlow_Medium_ttf), applause::Font::kLeft) {}

void RadioButton::setText(std::string text) {
    text_.setText(text);
    redraw();
}

void RadioButton::setFont(const applause::Font& font) {
    text_.setFont(font);
    redraw();
}

void RadioButton::draw(applause::Canvas& canvas) {
    const float hover = isActive() ? hover_amount_.update() : 0.0f;
    drawToggleIndicator(*this, canvas, text_, hover, canvas.value(ApplauseRadioButtonSize),
                        canvas.value(ApplauseRadioButtonTextGap), canvas.value(ApplauseRadioButtonBorderWidth), 0.0f, true);
    if (isActive() && hover_amount_.isAnimating()) redraw();
}

}  // namespace applause
