/* Copyright Vital Audio, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "ApplauseButton.h"

#include <embedded/applause_fonts.h>
#include <visage_graphics/theme.h>
#include <visage_windowing/windowing.h>

namespace applause {
// Button theme colors
VISAGE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonShadow, 0x88000000);

// ToggleButton theme colors
VISAGE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonDisabled,
                             0xff4c4f52);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOff, 0xff848789);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOffHover,
                             0xffaaacad);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOn, 0xffaa88ff);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOnHover,
                             0xffbb99ff);

// UiButton theme colors and values
// Regular button colors - Normal, Hover, Pressed
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBackground,
                             0x00000000);  // Transparent
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBackgroundHover,
                             0x00ffffff);  // Low opacity white
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBackgroundPressed,
                             0xffffffff);  // White

VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonText,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonTextHover,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonTextPressed,
                             0xff000000);  // Black

VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBorder,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBorderHover,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBorderPressed,
                             0xffffffff);  // White

// Action button colors - Normal, Hover, Pressed
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBackground,
                             0xff9977ee);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBackgroundHover,
                             0x00ffffff);  // Low opacity white
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBackgroundPressed,
                             0xffffffff);  // White

VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonText,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonTextHover,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonTextPressed,
                             0xff000000);  // Black

VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBorder,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBorderHover,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBorderPressed,
                             0xffffffff);  // White

// Misc values
VISAGE_THEME_IMPLEMENT_VALUE(UiButton, ApplauseUiButtonRounding, 1.0f);
VISAGE_THEME_IMPLEMENT_VALUE(UiButton, ApplauseUiButtonHoverRoundingMult, 1.0f);
VISAGE_THEME_IMPLEMENT_VALUE(UiButton, ApplauseUiButtonBorderWidth, 2.0f);
VISAGE_THEME_IMPLEMENT_VALUE(UiButton, ApplauseUiButtonBorderWidthHover, 4.0f);
VISAGE_THEME_IMPLEMENT_VALUE(UiButton, ApplauseUiButtonBorderWidthPressed,
                             4.0f);

// ToggleTextButton theme colors and values
// Background colors - Off state (normal, hover), On state (white like UIButton
// pressed)
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton,
                             ApplauseToggleTextButtonBackgroundOff,
                             0x00000000);  // Transparent
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton,
                             ApplauseToggleTextButtonBackgroundOffHover,
                             0x00ffffff);  // Low opacity white
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton,
                             ApplauseToggleTextButtonBackgroundOn,
                             0xffffffff);  // White (like UIButton pressed)
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton,
                             ApplauseToggleTextButtonBackgroundOnHover,
                             0xffffffff);  // White

// Text colors - Off state (white), On state (black like UIButton pressed)
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonTextOff,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton,
                             ApplauseToggleTextButtonTextOffHover,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonTextOn,
                             0xff000000);  // Black (like UIButton pressed)
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton,
                             ApplauseToggleTextButtonTextOnHover,
                             0xff000000);  // Black

// Border colors - matching UIButton style
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton,
                             ApplauseToggleTextButtonBorderOff,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton,
                             ApplauseToggleTextButtonBorderOffHover,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBorderOn,
                             0xffffffff);  // White
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton,
                             ApplauseToggleTextButtonBorderOnHover,
                             0xffffffff);  // White

// Values - matching UIButton style
VISAGE_THEME_IMPLEMENT_VALUE(ToggleTextButton, ApplauseToggleTextButtonRounding,
                             1.0f);
VISAGE_THEME_IMPLEMENT_VALUE(ToggleTextButton,
                             ApplauseToggleTextButtonHoverRoundingMult, 1.0f);
VISAGE_THEME_IMPLEMENT_VALUE(ToggleTextButton,
                             ApplauseToggleTextButtonBorderWidth, 2.0f);
VISAGE_THEME_IMPLEMENT_VALUE(ToggleTextButton,
                             ApplauseToggleTextButtonBorderWidthHover, 4.0f);

void Button::draw(visage::Canvas& canvas) {
    draw(canvas, active_ ? hover_amount_.update() : 0.0f);

    if (hover_amount_.isAnimating()) redraw();
}

void Button::mouseEnter(const visage::MouseEvent& e) {
    hover_amount_.target(true);
    if (set_pointer_cursor_ && active_)
        visage::setCursorStyle(visage::MouseCursor::Pointing);

    redraw();
}

void Button::mouseExit(const visage::MouseEvent& e) {
    hover_amount_.target(false);
    pressed_ = false;
    if (set_pointer_cursor_) visage::setCursorStyle(visage::MouseCursor::Arrow);

    redraw();
}

void Button::mouseDown(const visage::MouseEvent& e) {
    if (!active_) return;

    alt_clicked_ = e.isAltDown();
    pressed_ = true;
    hover_amount_.target(false);
    redraw();

    if (toggle_on_mouse_down_) notify(toggle());
}

void Button::mouseUp(const visage::MouseEvent& e) {
    if (!active_) return;

    pressed_ = false;
    redraw();
    if (localBounds().contains(e.position)) {
        hover_amount_.target(true, true);
        if (!toggle_on_mouse_down_) notify(toggle());
    }
}

UiButton::UiButton(const std::string& text)
    : text_(text,
            visage::Font(12, applause::fonts::JetBrainsMonoNL_SemiBold_ttf)) {}

UiButton::UiButton(const std::string& text, const visage::Font& font)
    : text_(text, font) {}

void UiButton::drawBackground(visage::Canvas& canvas, float hover_amount) {
    // Set background color based on pressed/hover state
    if (isPressed()) {
        if (action_)
            canvas.setColor(ApplauseUiActionButtonBackgroundPressed);
        else
            canvas.setColor(ApplauseUiButtonBackgroundPressed);
    } else {
        if (action_)
            canvas.setBlendedColor(ApplauseUiActionButtonBackground,
                                   ApplauseUiActionButtonBackgroundHover,
                                   hover_amount);
        else
            canvas.setBlendedColor(ApplauseUiButtonBackground,
                                   ApplauseUiButtonBackgroundHover,
                                   hover_amount);
    }

    int w = width();
    int h = height();
    float rounding = canvas.value(ApplauseUiButtonRounding);
    float mult =
        hover_amount * canvas.value(ApplauseUiButtonHoverRoundingMult) +
        (1.0f - hover_amount);

    canvas.roundedRectangle(0, 0, w, h, rounding * mult);

    // Set border color based on pressed/hover state
    if (isPressed()) {
        if (action_)
            canvas.setColor(ApplauseUiActionButtonBorderPressed);
        else
            canvas.setColor(ApplauseUiButtonBorderPressed);
        canvas.roundedRectangleBorder(
            0, 0, w, h, rounding * mult,
            canvas.value(ApplauseUiButtonBorderWidthPressed));
    } else {
        if (action_)
            canvas.setBlendedColor(ApplauseUiActionButtonBorder,
                                   ApplauseUiActionButtonBorderHover,
                                   hover_amount);
        else
            canvas.setBlendedColor(ApplauseUiButtonBorder,
                                   ApplauseUiButtonBorderHover, hover_amount);
        float border_width =
            hover_amount * canvas.value(ApplauseUiButtonBorderWidthHover) +
            (1.0f - hover_amount) * canvas.value(ApplauseUiButtonBorderWidth);
        canvas.roundedRectangleBorder(0, 0, w, h, rounding * mult,
                                      border_width);
    }
}

void UiButton::draw(visage::Canvas& canvas, float hover_amount) {
    drawBackground(canvas, hover_amount);

    // Set text color based on pressed/hover state
    if (isPressed()) {
        if (action_)
            canvas.setColor(ApplauseUiActionButtonTextPressed);
        else
            canvas.setColor(ApplauseUiButtonTextPressed);
    } else {
        if (action_)
            canvas.setBlendedColor(ApplauseUiActionButtonText,
                                   ApplauseUiActionButtonTextHover,
                                   hover_amount);
        else
            canvas.setBlendedColor(ApplauseUiButtonText,
                                   ApplauseUiButtonTextHover, hover_amount);
    }
    canvas.text(&text_, 0, 0, width(), height());
}

void IconButton::draw(visage::Canvas& canvas, float hover_amount) {
    shadow_.setFillBrush(canvas.color(Button::ApplauseButtonShadow));

    if (isActive())
        icon_.setFillBrush(canvas.blendedColor(
            ToggleButton::ApplauseToggleButtonOff,
            ToggleButton::ApplauseToggleButtonOffHover, hover_amount));
    else
        icon_.setFillBrush(canvas.color(ToggleButton::ApplauseToggleButtonDisabled));
}

bool ToggleButton::toggle() {
    toggled_ = !toggled_;

    if (undoable_) {
        auto change_action =
            std::make_unique<ButtonChangeAction>(this, toggled_);
        if (undoSetupFunction())
            change_action->setSetupFunction(undoSetupFunction());
        addUndoableAction(std::move(change_action));
    }
    toggleValueChanged();
    return toggled_;
}

void ToggleIconButton::draw(visage::Canvas& canvas, float hover_amount) {
    shadow_.setFillBrush(canvas.color(Button::ApplauseButtonShadow));

    if (toggled())
        icon_.setFillBrush(canvas.blendedColor(ApplauseToggleButtonOn,
                                                ApplauseToggleButtonOnHover,
                                                hover_amount));
    else
        icon_.setFillBrush(canvas.blendedColor(ApplauseToggleButtonOff,
                                                ApplauseToggleButtonOffHover,
                                                hover_amount));
}

ToggleTextButton::ToggleTextButton(const std::string& name)
    : ToggleButton(name),
      text_(name,
            visage::Font(12, applause::fonts::JetBrainsMonoNL_SemiBold_ttf)) {}

ToggleTextButton::ToggleTextButton(const std::string& name,
                                   const visage::Font& font)
    : ToggleButton(name), text_(name, font) {}

void ToggleTextButton::drawBackground(visage::Canvas& canvas,
                                      float hover_amount) {
    // Set background color based on toggled/hover state
    if (toggled())
        canvas.setBlendedColor(ApplauseToggleTextButtonBackgroundOn,
                               ApplauseToggleTextButtonBackgroundOnHover,
                               hover_amount);
    else
        canvas.setBlendedColor(ApplauseToggleTextButtonBackgroundOff,
                               ApplauseToggleTextButtonBackgroundOffHover,
                               hover_amount);

    int w = width();
    int h = height();
    float rounding = canvas.value(ApplauseToggleTextButtonRounding);
    float mult =
        hover_amount * canvas.value(ApplauseToggleTextButtonHoverRoundingMult) +
        (1.0f - hover_amount);

    canvas.roundedRectangle(0, 0, w, h, rounding * mult);

    // Set border color based on toggled/hover state
    if (toggled())
        canvas.setBlendedColor(ApplauseToggleTextButtonBorderOn,
                               ApplauseToggleTextButtonBorderOnHover,
                               hover_amount);
    else
        canvas.setBlendedColor(ApplauseToggleTextButtonBorderOff,
                               ApplauseToggleTextButtonBorderOffHover,
                               hover_amount);

    float border_width =
        hover_amount * canvas.value(ApplauseToggleTextButtonBorderWidthHover) +
        (1.0f - hover_amount) *
            canvas.value(ApplauseToggleTextButtonBorderWidth);
    canvas.roundedRectangleBorder(0, 0, w, h, rounding * mult, border_width);
}

void ToggleTextButton::draw(visage::Canvas& canvas, float hover_amount) {
    if (draw_background_) drawBackground(canvas, hover_amount);

    if (toggled())
        canvas.setBlendedColor(ApplauseToggleTextButtonTextOn,
                               ApplauseToggleTextButtonTextOnHover,
                               hover_amount);
    else
        canvas.setBlendedColor(ApplauseToggleTextButtonTextOff,
                               ApplauseToggleTextButtonTextOffHover,
                               hover_amount);
    canvas.text(&text_, 0, 0, width(), height());
}
}  // namespace applause