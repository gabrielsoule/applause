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

#include "Button.h"

#include <applause/ui/ApplauseUI.h>

#include <applause/ui/ApplauseEditor.h>
#include <applause/ui/NativePopupMenu.h>

#include <embedded/applause_fonts.h>

namespace applause {
// Button base class — shared colors and values
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonShadow, 0x88000000);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonGlow, 0xff9966ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonBackgroundTop, 0xff333333);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonBackgroundBottom, 0xff222222);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonBackgroundTopHover, 0xff3e3e42);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonBackgroundBottomHover, 0xff2e2e32);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonText, 0xffcccccc);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonTextHover, 0xffdddddd);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonTextPressed, 0xffffffff);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonBorder, 0xff444444);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonBorderHover, 0xff5a5a60);
APPLAUSE_THEME_IMPLEMENT_COLOR(Button, ApplauseButtonBorderPressed, 0xff555555);
APPLAUSE_THEME_IMPLEMENT_VALUE(Button, ApplauseButtonRounding, 7.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Button, ApplauseButtonHoverRoundingMult, 1.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Button, ApplauseButtonBorderWidth, 1.5f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Button, ApplauseButtonBorderWidthHover, 1.5f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Button, ApplauseButtonBorderWidthPressed, 1.5f);

// ToggleButton theme colors
APPLAUSE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonDisabled, 0xff4c4f52);
APPLAUSE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOff, 0xff848789);
APPLAUSE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOffHover, 0xffaaacad);
APPLAUSE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOn, 0xff9966ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(ToggleButton, ApplauseToggleButtonOnHover, 0xffaa77ff);

// UiButton — action-specific colors only
APPLAUSE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseActionButtonBackgroundTop, 0xff8855ee);
APPLAUSE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseActionButtonBackgroundBottom, 0xff6633cc);
APPLAUSE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseActionButtonBackgroundTopHover, 0xff9966ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseActionButtonBackgroundBottomHover, 0xff7744dd);
APPLAUSE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseActionButtonBorder, 0xff9966ee);
APPLAUSE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseActionButtonBorderHover, 0xffaa77ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(UiButton, ApplauseActionButtonBorderPressed, 0xffffffff);

// ToggleTextButton — ON-state accent colors only
APPLAUSE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonTextOn, 0xdd9966ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonTextOnHover, 0xffaa77ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonGlow, 0x359966ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBorderOn, 0xbb9966ff);
APPLAUSE_THEME_IMPLEMENT_COLOR(ToggleTextButton, ApplauseToggleTextButtonBorderOnHover, 0xddaa77ff);

void Button::draw(applause::Canvas& canvas) {
    float hover = active_ ? hover_amount_.update() : 0.0f;
    draw(canvas, hover);

    if (hover > 0.0f) {
        applause::Color accent = canvas.color(ApplauseButtonGlow).gradient().sample(0.0f);
        applause::Point center = {width() * 0.5f, height() * 0.5f};
        canvas.setColor(applause::Brush::radial(accent.withAlpha(hover * 0.30f), applause::Color(0x00000000), center,
                                              width() * 0.5f, height() * 0.5f));
        canvas.rectangle(0, 0, width(), height());
    }

    if (hover_amount_.isAnimating()) redraw();
}

void Button::mouseEnter(const applause::MouseEvent& e) {
    hover_amount_.target(true);
    if (set_pointer_cursor_ && active_) applause::setCursorStyle(applause::MouseCursor::Pointing);

    redraw();
}

void Button::mouseExit(const applause::MouseEvent& e) {
    hover_amount_.target(false);
    pressed_ = false;
    if (set_pointer_cursor_) applause::setCursorStyle(applause::MouseCursor::Arrow);

    redraw();
}

void Button::mouseDown(const applause::MouseEvent& e) {
    if (!active_) return;

    alt_clicked_ = e.isAltDown();
    pressed_ = true;
    hover_amount_.target(false);
    redraw();

    if (toggle_on_mouse_down_) notify(toggle());
}

void Button::mouseUp(const applause::MouseEvent& e) {
    if (!active_) return;

    pressed_ = false;
    redraw();
    if (localBounds().contains(e.position)) {
        hover_amount_.target(true, true);
        if (!toggle_on_mouse_down_) notify(toggle());
    }
}

UiButton::UiButton(const std::string& text) : text_(text, applause::Font(12, applause::fonts::Jost_Regular_ttf)) {}

UiButton::UiButton(const std::string& text, const applause::Font& font) : text_(text, font) {}

void UiButton::drawBackground(applause::Canvas& canvas, float hover_amount) {
    int w = width();
    int h = height();
    float rounding = canvas.value(ApplauseButtonRounding);
    float mult = hover_amount * canvas.value(ApplauseButtonHoverRoundingMult) + (1.0f - hover_amount);
    float r = rounding * mult;

    auto sample = [&](auto id) { return canvas.color(id).gradient().sample(0.0f); };

    if (!isActive()) {
        constexpr float kInactiveDim = 0.2f;
        applause::Color black(0xff000000);
        canvas.setColor(
            applause::Brush::vertical(sample(ApplauseButtonBackgroundTop).interpolateWith(black, kInactiveDim),
                                    sample(ApplauseButtonBackgroundBottom).interpolateWith(black, kInactiveDim)));
        canvas.roundedRectangle(0, 0, w, h, r);
        canvas.setColor(sample(ApplauseButtonBorder).interpolateWith(black, kInactiveDim));
        canvas.roundedRectangleBorder(0, 0, w, h, r, canvas.value(ApplauseButtonBorderWidth));
        return;
    }

    if (isPressed()) {
        if (action_) {
            // Action pressed: invert the action gradient
            canvas.setColor(applause::Brush::vertical(sample(ApplauseActionButtonBackgroundBottom),
                                                    sample(ApplauseActionButtonBackgroundTop)));
        } else {
            // Regular pressed: invert the shared gradient
            canvas.setColor(
                applause::Brush::vertical(sample(ApplauseButtonBackgroundBottom), sample(ApplauseButtonBackgroundTop)));
        }
    } else {
        if (action_) {
            applause::Brush normal = applause::Brush::vertical(sample(ApplauseActionButtonBackgroundTop),
                                                           sample(ApplauseActionButtonBackgroundBottom));
            applause::Brush hover = applause::Brush::vertical(sample(ApplauseActionButtonBackgroundTopHover),
                                                          sample(ApplauseActionButtonBackgroundBottomHover));
            canvas.setColor(normal.interpolateWith(hover, hover_amount));
        } else {
            applause::Brush normal =
                applause::Brush::vertical(sample(ApplauseButtonBackgroundTop), sample(ApplauseButtonBackgroundBottom));
            applause::Brush hover = applause::Brush::vertical(sample(ApplauseButtonBackgroundTopHover),
                                                          sample(ApplauseButtonBackgroundBottomHover));
            canvas.setColor(normal.interpolateWith(hover, hover_amount));
        }
    }
    canvas.roundedRectangle(0, 0, w, h, r);

    if (isPressed()) {
        if (action_)
            canvas.setColor(ApplauseActionButtonBorderPressed);
        else
            canvas.setColor(ApplauseButtonBorderPressed);
        canvas.roundedRectangleBorder(0, 0, w, h, r, canvas.value(ApplauseButtonBorderWidthPressed));
    } else {
        if (action_)
            canvas.setBlendedColor(ApplauseActionButtonBorder, ApplauseActionButtonBorderHover, hover_amount);
        else
            canvas.setBlendedColor(ApplauseButtonBorder, ApplauseButtonBorderHover, hover_amount);
        float border_width = hover_amount * canvas.value(ApplauseButtonBorderWidthHover) +
            (1.0f - hover_amount) * canvas.value(ApplauseButtonBorderWidth);
        canvas.roundedRectangleBorder(0, 0, w, h, r, border_width);
    }
}

void UiButton::draw(applause::Canvas& canvas, float hover_amount) {
    drawBackground(canvas, hover_amount);

    if (!isActive()) {
        applause::Color text_color = canvas.color(ApplauseButtonText).gradient().sample(0.0f);
        canvas.setColor(text_color.interpolateWith(applause::Color(0xff000000), 0.5f));
    } else if (isPressed())
        canvas.setColor(ApplauseButtonTextPressed);
    else
        canvas.setBlendedColor(ApplauseButtonText, ApplauseButtonTextHover, hover_amount);

    canvas.text(&text_, 0, 0, width(), height());
}

void IconButton::draw(applause::Canvas& canvas, float hover_amount) {
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

void ToggleIconButton::draw(applause::Canvas& canvas, float hover_amount) {
    shadow_.setFillBrush(canvas.color(Button::ApplauseButtonShadow));

    if (toggled())
        icon_.setFillBrush(canvas.blendedColor(ApplauseToggleButtonOn, ApplauseToggleButtonOnHover, hover_amount));
    else
        icon_.setFillBrush(canvas.blendedColor(ApplauseToggleButtonOff, ApplauseToggleButtonOffHover, hover_amount));
}

ToggleTextButton::ToggleTextButton(const std::string& name) :
    ToggleButton(name), text_(name, applause::Font(12, applause::fonts::Jost_Regular_ttf)) {}

ToggleTextButton::ToggleTextButton(const std::string& name, const applause::Font& font) :
    ToggleButton(name), text_(name, font) {}

void ToggleTextButton::drawBackground(applause::Canvas& canvas, float hover_amount) {
    int w = width();
    int h = height();
    float rounding = canvas.value(ApplauseButtonRounding);
    float mult = hover_amount * canvas.value(ApplauseButtonHoverRoundingMult) + (1.0f - hover_amount);
    float r = rounding * mult;

    // super annoying workaround for getting a color out of a Brush. There's not a better way right now.
    auto sample = [&](auto id) { return canvas.color(id).gradient().sample(0.0f); };

    if (!isActive()) {
        constexpr float kInactiveDim = 0.2f;
        applause::Color black(0xff000000);
        canvas.setColor(
            applause::Brush::vertical(sample(ApplauseButtonBackgroundTop).interpolateWith(black, kInactiveDim),
                                    sample(ApplauseButtonBackgroundBottom).interpolateWith(black, kInactiveDim)));
        canvas.roundedRectangle(0, 0, w, h, r);
        canvas.setColor(sample(ApplauseButtonBorder).interpolateWith(black, kInactiveDim));
        canvas.roundedRectangleBorder(0, 0, w, h, r, canvas.value(ApplauseButtonBorderWidth));
        return;
    }

    applause::Brush normal =
        applause::Brush::vertical(sample(ApplauseButtonBackgroundTop), sample(ApplauseButtonBackgroundBottom));
    applause::Brush hover =
        applause::Brush::vertical(sample(ApplauseButtonBackgroundTopHover), sample(ApplauseButtonBackgroundBottomHover));
    canvas.setColor(normal.interpolateWith(hover, hover_amount));
    canvas.roundedRectangle(0, 0, w, h, r);

    if (toggled()) {
        applause::Point center = {w * 0.5f, h * 0.5f};
        applause::Color glow_color = canvas.color(ApplauseToggleTextButtonGlow).gradient().sample(0.0f);
        canvas.setColor(applause::Brush::radial(applause::Color(0x00000000), glow_color, center, w * 0.5f, h * 0.5f));
        canvas.roundedRectangle(0, 0, w, h, r);
    }

    if (toggled())
        canvas.setBlendedColor(ApplauseToggleTextButtonBorderOn, ApplauseToggleTextButtonBorderOnHover, hover_amount);
    else
        canvas.setBlendedColor(ApplauseButtonBorder, ApplauseButtonBorderHover, hover_amount);

    float border_width = hover_amount * canvas.value(ApplauseButtonBorderWidthHover) +
        (1.0f - hover_amount) * canvas.value(ApplauseButtonBorderWidth);
    canvas.roundedRectangleBorder(0, 0, w, h, r, border_width);
}

void ToggleTextButton::draw(applause::Canvas& canvas, float hover_amount) {
    if (draw_background_) drawBackground(canvas, hover_amount);

    if (!isActive()) {
        applause::Color text_color = canvas.color(ApplauseButtonText).gradient().sample(0.0f);
        canvas.setColor(text_color.interpolateWith(applause::Color(0xff000000), 0.5f));
    } else if (toggled())
        canvas.setBlendedColor(ApplauseToggleTextButtonTextOn, ApplauseToggleTextButtonTextOnHover, hover_amount);
    else
        canvas.setBlendedColor(ApplauseButtonText, ApplauseButtonTextHover, hover_amount);

    canvas.text(&text_, 0, 0, width(), height());
}
PopupMenuButton::PopupMenuButton(const std::string& default_text) : UiButton(default_text) {
    onToggle() += [this](Button*, bool) { showPopup(); };
}

void PopupMenuButton::showPopup() {
    if (!on_build_menu_) return;

    auto* editor = findParent<applause::ApplauseEditor>();
    if (!editor) return;

    void* native_handle = editor->getNativeHandle();
    if (!native_handle) return;

    NativePopupMenu menu("Select");
    on_build_menu_(menu);

    menu.onSelection() += [this](int id) { on_item_selected_.callback(id); };

    auto pos = positionInWindow();
    menu.show(native_handle, pos.x, pos.y + height());
}

}  // namespace applause
