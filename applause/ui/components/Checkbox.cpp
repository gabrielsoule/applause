#include "Checkbox.h"

#include "RadioButton.h"

#include <algorithm>

namespace applause {

namespace {

void drawToggleIndicator(Button& button, applause::Canvas& canvas, float hover, float indicator_size, float text_gap,
                         float border_width, float rounding, bool radio) {
    const float size = std::max(0.0f, std::min({indicator_size, button.width(), button.height()}));
    const float y = (button.height() - size) * 0.5f;
    const float border = std::clamp(border_width, 0.0f, size * 0.5f);
    const applause::Color black(0xff000000);
    rounding = std::clamp(rounding, 0.0f, size * 0.5f);

    if (size > 0.0f) {
        canvas.setColor(
            color(button, Button::kBackground)
                .interpolateWith(applause::Color(0xffffffff), hover * value(button, Button::kHoverLighten)));
        if (radio)
            canvas.circle(0.0f, y, size);
        else
            canvas.roundedRectangle(0.0f, y, size, size, rounding);

        const float glow = std::min(1.0f, 1.75f * hover * value(button, Button::kGlowAmount));
        if (glow > 0.0f) {
            const auto accent = color(button, Button::kGlow);
            const applause::Point center{size * 0.5f, y + size * 0.5f};
            canvas.setColor(applause::Brush::radial(accent.withAlpha(glow), applause::Color(0x00000000), center,
                                                  size * 0.65f));
            if (radio)
                canvas.circle(0.0f, y, size);
            else
                canvas.roundedRectangle(0.0f, y, size, size, rounding);
        }

        applause::Color outline;
        if (button.isPressed())
            outline = color(button, Button::kTextPressed);
        else if (button.isChecked())
            outline = color(button, Button::kOn);
        else
            outline = color(button, Button::kOff).interpolateWith(color(button, Button::kOffHover), hover);
        canvas.setColor(button.isEnabled() ? outline : outline.interpolateWith(black, 0.5f));

        if (radio)
            canvas.ring(0.0f, y, size, border);
        else
            canvas.roundedRectangleBorder(0.0f, y, size, size, rounding, border);

        if (button.isChecked()) {
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

    applause::Color label = button.isPressed()
        ? color(button, Button::kTextPressed)
        : color(button, Button::kText).interpolateWith(color(button, Button::kTextHover), hover);
    canvas.setColor(button.isEnabled() ? label : label.interpolateWith(black, 0.5f));

    const float text_x = size + std::max(0.0f, text_gap);
    const float text_width = std::max(0.0f, button.width() - text_x);
    if (!button.text().isEmpty() && text_width > 0.0f)
        canvas.text(button.text(), button.font(), applause::Font::kLeft, text_x, 0.0f, text_width, button.height());
}

}  // namespace

Checkbox::Checkbox(std::string text) : Button(std::move(text)) { setCheckable(true); }

void Checkbox::drawContent(applause::Canvas& canvas, float hover) {
    drawToggleIndicator(*this, canvas, hover, value(*this, kSize), value(*this, kTextGap), value(*this, kBorderWidth),
                        value(*this, kRounding), false);
}

RadioButton::RadioButton(std::string text) : Button(std::move(text)) { setCheckable(true); }

void RadioButton::drawContent(applause::Canvas& canvas, float hover) {
    drawToggleIndicator(*this, canvas, hover, value(*this, kSize), value(*this, kTextGap), value(*this, kBorderWidth),
                        0.0f, true);
}

}  // namespace applause
