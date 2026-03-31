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
VISAGE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonGlow, 0xff9966ff);

// ToggleButton theme colors
VISAGE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonDisabled, 0xff4c4f52);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOff, 0xff848789);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOffHover, 0xffaaacad);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOn, 0xff9966ff);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOnHover, 0xffaa77ff);

// UiButton theme colors and values
// Regular button background gradient - Normal, Hover, Pressed
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBackgroundTop, 0xff333333);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBackgroundBottom, 0xff222222);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBackgroundTopHover, 0xff3e3e42);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBackgroundBottomHover, 0xff2e2e32);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBackgroundTopPressed, 0xff222222);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBackgroundBottomPressed, 0xff333333);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonText, 0xffcccccc);  // #ccc
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonTextHover, 0xffdddddd);  // #ddd
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonTextPressed, 0xffffffff);  // #fff

VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBorder, 0xff444444);  // #444
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBorderHover, 0xff5a5a60);  // #5a5a60
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiButtonBorderPressed, 0xff555555);  // #555

// Action button background gradient - Normal, Hover, Pressed
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBackgroundTop, 0xff8855ee);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBackgroundBottom, 0xff6633cc);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBackgroundTopHover, 0xff9966ff);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBackgroundBottomHover, 0xff7744dd);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBackgroundTopPressed, 0xff6633cc);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBackgroundBottomPressed, 0xff8855ee);

VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonText, 0xffcccccc);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonTextHover, 0xffdddddd);
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonTextPressed, 0xffffffff);

VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBorder, 0xff9966ee);  // Purple border
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBorderHover, 0xffaa77ff);  // Brighter purple border
VISAGE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseUiActionButtonBorderPressed, 0xffffffff);  // White

// ToggleTextButton background gradient - Off state
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBackgroundOffTop, 0xff333333);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBackgroundOffBottom, 0xff222222);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBackgroundOffHoverTop, 0xff3e3e42);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBackgroundOffHoverBottom, 0xff2e2e32);
// ToggleTextButton background gradient - On state
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBackgroundOnTop, 0xff333333);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBackgroundOnBottom, 0xff222222);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBackgroundOnHoverTop, 0xff3e3e42);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBackgroundOnHoverBottom, 0xff2e2e32);

// Text colors - Off state (white), On state (black like UIButton pressed)
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonTextOff,
                             0xffcccccc);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonTextOffHover,
                             0xffdddddd);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonTextOn,
                             0xdd9966ff);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonTextOnHover,
                             0xffaa77ff);

// Glow color (radial glow overlay when toggled on)
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonGlow,
                             0x359966ff);  // Rich purple at low alpha

// Border colors - matching UIButton style
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBorderOff,
                             0xff444444);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBorderOffHover,
                             0xff5a5a60);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBorderOn,
                             0xbb9966ff);
VISAGE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBorderOnHover,
                             0xddaa77ff);

VISAGE_THEME_IMPLEMENT_VALUE(ToggleTextButton, ApplauseToggleTextButtonRounding, 5.0f);
VISAGE_THEME_IMPLEMENT_VALUE(ToggleTextButton, ApplauseToggleTextButtonHoverRoundingMult, 1.0f);
VISAGE_THEME_IMPLEMENT_VALUE(ToggleTextButton, ApplauseToggleTextButtonBorderWidth, 1.5f);
VISAGE_THEME_IMPLEMENT_VALUE(ToggleTextButton, ApplauseToggleTextButtonBorderWidthHover, 1.5f);

VISAGE_THEME_IMPLEMENT_VALUE(UiButton, ApplauseUiButtonRounding, 5.0f);
VISAGE_THEME_IMPLEMENT_VALUE(UiButton, ApplauseUiButtonHoverRoundingMult, 1.0f);
VISAGE_THEME_IMPLEMENT_VALUE(UiButton, ApplauseUiButtonBorderWidth, 1.5f);
VISAGE_THEME_IMPLEMENT_VALUE(UiButton, ApplauseUiButtonBorderWidthHover, 1.5f);
VISAGE_THEME_IMPLEMENT_VALUE(UiButton, ApplauseUiButtonBorderWidthPressed, 1.5f);

void Button::draw(visage::Canvas& canvas) {
    float hover = active_ ? hover_amount_.update() : 0.0f;
    draw(canvas, hover);

    if (hover > 0.0f) {
        visage::Color accent = canvas.color(ApplauseButtonGlow).gradient().sample(0.0f);
        visage::Point center = {width() * 0.5f, height() * 0.5f};
        canvas.setColor(visage::Brush::radial(accent.withAlpha(hover * 0.30f),
                                              visage::Color(0x00000000),
                                              center, width() * 0.5f, height() * 0.5f));
        canvas.rectangle(0, 0, width(), height());
    }

    if (hover_amount_.isAnimating()) redraw();
}

void Button::mouseEnter(const visage::MouseEvent& e) {
    hover_amount_.target(true);
    if (set_pointer_cursor_ && active_) visage::setCursorStyle(visage::MouseCursor::Pointing);

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

UiButton::UiButton(const std::string& text) : text_(text, visage::Font(12, applause::fonts::Jost_Regular_ttf)) {}

UiButton::UiButton(const std::string& text, const visage::Font& font) : text_(text, font) {}

void UiButton::drawBackground(visage::Canvas& canvas, float hover_amount) {
    int w = width();
    int h = height();
    float rounding = canvas.value(ApplauseUiButtonRounding);
    float mult = hover_amount * canvas.value(ApplauseUiButtonHoverRoundingMult) + (1.0f - hover_amount);
    float r = rounding * mult;

    auto sample = [&](auto id) { return canvas.color(id).gradient().sample(0.0f); };

    if (isPressed()) {
        if (action_)
            canvas.setColor(visage::Brush::vertical(sample(ApplauseUiActionButtonBackgroundTopPressed),
                                                    sample(ApplauseUiActionButtonBackgroundBottomPressed)));
        else
            canvas.setColor(visage::Brush::vertical(sample(ApplauseUiButtonBackgroundTopPressed),
                                                    sample(ApplauseUiButtonBackgroundBottomPressed)));
    } else {
        if (action_) {
            visage::Brush normal = visage::Brush::vertical(sample(ApplauseUiActionButtonBackgroundTop),
                                                           sample(ApplauseUiActionButtonBackgroundBottom));
            visage::Brush hover = visage::Brush::vertical(sample(ApplauseUiActionButtonBackgroundTopHover),
                                                          sample(ApplauseUiActionButtonBackgroundBottomHover));
            canvas.setColor(normal.interpolateWith(hover, hover_amount));
        } else {
            visage::Brush normal = visage::Brush::vertical(sample(ApplauseUiButtonBackgroundTop),
                                                           sample(ApplauseUiButtonBackgroundBottom));
            visage::Brush hover = visage::Brush::vertical(sample(ApplauseUiButtonBackgroundTopHover),
                                                          sample(ApplauseUiButtonBackgroundBottomHover));
            canvas.setColor(normal.interpolateWith(hover, hover_amount));
        }
    }
    canvas.roundedRectangle(0, 0, w, h, r);

    if (isPressed()) {
        if (action_)
            canvas.setColor(ApplauseUiActionButtonBorderPressed);
        else
            canvas.setColor(ApplauseUiButtonBorderPressed);
        canvas.roundedRectangleBorder(0, 0, w, h, r, canvas.value(ApplauseUiButtonBorderWidthPressed));
    } else {
        if (action_)
            canvas.setBlendedColor(ApplauseUiActionButtonBorder, ApplauseUiActionButtonBorderHover, hover_amount);
        else
            canvas.setBlendedColor(ApplauseUiButtonBorder, ApplauseUiButtonBorderHover, hover_amount);
        float border_width = hover_amount * canvas.value(ApplauseUiButtonBorderWidthHover) +
            (1.0f - hover_amount) * canvas.value(ApplauseUiButtonBorderWidth);
        canvas.roundedRectangleBorder(0, 0, w, h, r, border_width);
    }
}

void UiButton::draw(visage::Canvas& canvas, float hover_amount) {
    drawBackground(canvas, hover_amount);

    if (isPressed()) {
        if (action_)
            canvas.setColor(ApplauseUiActionButtonTextPressed);
        else
            canvas.setColor(ApplauseUiButtonTextPressed);
    } else {
        if (action_)
            canvas.setBlendedColor(ApplauseUiActionButtonText, ApplauseUiActionButtonTextHover, hover_amount);
        else
            canvas.setBlendedColor(ApplauseUiButtonText, ApplauseUiButtonTextHover, hover_amount);
    }
    canvas.text(&text_, 0, 0, width(), height());
}

void IconButton::draw(visage::Canvas& canvas, float hover_amount) {
    shadow_.setFillBrush(canvas.color(Button::ApplauseButtonShadow));

    if (isActive())
        icon_.setFillBrush(canvas.blendedColor(ToggleButton::ApplauseToggleButtonOff,
                                               ToggleButton::ApplauseToggleButtonOffHover, hover_amount));
    else
        icon_.setFillBrush(canvas.color(ToggleButton::ApplauseToggleButtonDisabled));
}

bool ToggleButton::toggle() {
    toggled_ = !toggled_;

    if (undoable_) {
        auto change_action = std::make_unique<ButtonChangeAction>(this, toggled_);
        if (undoSetupFunction()) change_action->setSetupFunction(undoSetupFunction());
        addUndoableAction(std::move(change_action));
    }
    toggleValueChanged();
    return toggled_;
}

void ToggleIconButton::draw(visage::Canvas& canvas, float hover_amount) {
    shadow_.setFillBrush(canvas.color(Button::ApplauseButtonShadow));

    if (toggled())
        icon_.setFillBrush(canvas.blendedColor(ApplauseToggleButtonOn, ApplauseToggleButtonOnHover, hover_amount));
    else
        icon_.setFillBrush(canvas.blendedColor(ApplauseToggleButtonOff, ApplauseToggleButtonOffHover, hover_amount));
}

ToggleTextButton::ToggleTextButton(const std::string& name) :
    ToggleButton(name), text_(name, visage::Font(12, applause::fonts::Jost_Regular_ttf)) {}

ToggleTextButton::ToggleTextButton(const std::string& name, const visage::Font& font) :
    ToggleButton(name), text_(name, font) {}

void ToggleTextButton::drawBackground(visage::Canvas& canvas, float hover_amount) {
    int w = width();
    int h = height();
    float rounding = canvas.value(ApplauseToggleTextButtonRounding);
    float mult = hover_amount * canvas.value(ApplauseToggleTextButtonHoverRoundingMult) + (1.0f - hover_amount);
    float r = rounding * mult;

    // Draw base background gradient
    auto sample = [&](auto id) { return canvas.color(id).gradient().sample(0.0f); };

    if (toggled()) {
        visage::Brush normal = visage::Brush::vertical(sample(ApplauseToggleTextButtonBackgroundOnTop),
                                                       sample(ApplauseToggleTextButtonBackgroundOnBottom));
        visage::Brush hover = visage::Brush::vertical(sample(ApplauseToggleTextButtonBackgroundOnHoverTop),
                                                      sample(ApplauseToggleTextButtonBackgroundOnHoverBottom));
        canvas.setColor(normal.interpolateWith(hover, hover_amount));
    } else {
        visage::Brush normal = visage::Brush::vertical(sample(ApplauseToggleTextButtonBackgroundOffTop),
                                                       sample(ApplauseToggleTextButtonBackgroundOffBottom));
        visage::Brush hover = visage::Brush::vertical(sample(ApplauseToggleTextButtonBackgroundOffHoverTop),
                                                      sample(ApplauseToggleTextButtonBackgroundOffHoverBottom));
        canvas.setColor(normal.interpolateWith(hover, hover_amount));
    }
    canvas.roundedRectangle(0, 0, w, h, r);

    if (toggled()) {
        visage::Point center = {w * 0.5f, h * 0.5f};
        visage::Color glow_color = canvas.color(ApplauseToggleTextButtonGlow).gradient().sample(0.0f);
        canvas.setColor(visage::Brush::radial(visage::Color(0x00000000), glow_color, center, w * 0.5f, h * 0.5f));
        canvas.roundedRectangle(0, 0, w, h, r);
    }

    // Draw border
    if (toggled())
        canvas.setBlendedColor(ApplauseToggleTextButtonBorderOn, ApplauseToggleTextButtonBorderOnHover, hover_amount);
    else
        canvas.setBlendedColor(ApplauseToggleTextButtonBorderOff, ApplauseToggleTextButtonBorderOffHover, hover_amount);

    float border_width = hover_amount * canvas.value(ApplauseToggleTextButtonBorderWidthHover) +
        (1.0f - hover_amount) * canvas.value(ApplauseToggleTextButtonBorderWidth);
    canvas.roundedRectangleBorder(0, 0, w, h, r, border_width);
}

void ToggleTextButton::draw(visage::Canvas& canvas, float hover_amount) {
    if (draw_background_) drawBackground(canvas, hover_amount);

    if (toggled()) {
        canvas.setBlendedColor(ApplauseToggleTextButtonTextOn, ApplauseToggleTextButtonTextOnHover, hover_amount);
    } else {
        canvas.setBlendedColor(ApplauseToggleTextButtonTextOff, ApplauseToggleTextButtonTextOffHover, hover_amount);
    }
    canvas.text(&text_, 0, 0, width(), height());
}
}  // namespace applause
