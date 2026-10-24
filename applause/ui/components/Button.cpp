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

#include "RadioGroup.h"

#include <applause/ui/ApplauseEditor.h>
#include <applause/ui/NativePopupMenu.h>

#include <embedded/applause_fonts.h>

#include <algorithm>

namespace applause {
namespace {
float textWidth(const applause::Text& text, float dpi_scale) {
    return text.font().withDpiScale(dpi_scale).stringWidth(text.text().c_str(), text.text().length());
}
}  // namespace

Button::Button(std::string text) : Frame(text), text_(text, applause::Font(12, applause::fonts::Barlow_Medium_ttf)) {
    hover_amount_.setTargetValue(1.0f);
    hover_amount_.setAnimationTime(200);
}

Button::Button(const Svg& icon, std::string text) : Button(std::move(text)) { setIcon(icon); }

Button::~Button() {
    if (radio_group_) radio_group_->buttonDestroyed(*this);
}

void Button::setText(std::string text) {
    text_.setText(text);
    placeIcon();
    redraw();
}

void Button::setFont(const Font& font) {
    text_.setFont(font);
    placeIcon();
    redraw();
}

void Button::setIcon(const Svg& icon) {
    if (!icon_) {
        icon_ = std::make_unique<applause::SvgFrame>();
        addChild(icon_.get());
    }
    icon_->load(icon);
    placeIcon();
}

void Button::setRole(Role role) {
    role_ = role;
    redraw();
}

void Button::setEnabled(bool enabled) {
    enabled_ = enabled;
    redraw();
}

void Button::setCheckable(bool checkable) {
    APPLAUSE_ASSERT(checkable || !radio_group_);
    if (!checkable) changeChecked(false, false);
    checkable_ = checkable;
}

void Button::setChecked(bool checked) {
    APPLAUSE_ASSERT(checkable_);
    if (checkable_) changeChecked(checked, false);
}

void Button::setCheckedAndNotify(bool checked) {
    APPLAUSE_ASSERT(checkable_);
    if (checkable_) changeChecked(checked, true);
}

void Button::changeChecked(bool checked, bool notify) {
    if (radio_group_) {
        radio_group_->setButtonState(*this, checked, notify);
        return;
    }
    if (checked_ == checked) return;

    checked_ = checked;
    dispatchCheckedChanged(notify);
    if (notify) on_toggle_.callback(checked_);
}

void Button::dispatchCheckedChanged(bool notify) {
    redraw();
    checkedChanged(notify);
}

void Button::click() {
    if (checkable_) changeChecked(!checked_, true);
    on_click_.callback();
}

float Button::preferredWidth() const {
    float result = 2.0f * value(*this, kPadding) + textWidth(text_, dpiScale());
    if (icon_) result += value(*this, kIconSize) + (text_.text().isEmpty() ? 0.0f : value(*this, kIconGap));
    return result;
}

void Button::placeIcon() {
    if (!icon_) return;

    const float size = std::max(0.0f, std::min({value(*this, kIconSize), width(), height()}));
    float content_width = size;
    if (!text_.text().isEmpty()) content_width += value(*this, kIconGap) + textWidth(text_, dpiScale());
    icon_->setBounds((width() - content_width) * 0.5f, (height() - size) * 0.5f, size, size);
}

float Button::updateHover() {
    if (!enabled_) return 0.0f;

    const float hover = hover_amount_.update();
    if (hover_amount_.isAnimating()) redraw();
    return hover;
}

void Button::draw(applause::Canvas& canvas) {
    const float hover = updateHover();
    drawBackground(canvas, hover);

    const Bounds content = contentBounds();
    canvas.saveState();
    canvas.trimClampBounds(content.x(), content.y(), content.width(), content.height());
    drawContent(canvas, hover);
    canvas.restoreState();
}

void Button::drawBackground(applause::Canvas& canvas, float hover) {
    const float w = width();
    const float h = height();
    const float rounding = value(*this, kRounding);
    const applause::Point center = {w * 0.5f, h * 0.5f};
    const applause::Color white(0xffffffff);
    const applause::Color black(0xff000000);
    const bool primary = role_ == Role::Primary;

    applause::Color border;
    if (role_ != Role::Flat) {
        const float lighten = hover * value(*this, kHoverLighten);
        applause::Color top = color(*this, primary ? kPrimary : kBackground).interpolateWith(white, lighten);
        border = primary ? top : color(*this, kBorder).interpolateWith(white, lighten);
        if (!enabled_) {
            top = top.interpolateWith(black, 0.2f);
            border = border.interpolateWith(black, 0.2f);
        }

        // Pressing flips the shading.
        const applause::Color bottom = top.interpolateWith(black, value(*this, kShade));
        canvas.setColor(pressed_ ? applause::Brush::vertical(bottom, top) : applause::Brush::vertical(top, bottom));
        canvas.roundedRectangle(0, 0, w, h, rounding);
    }

    if (checked_) {
        const applause::Color glow = color(*this, kOn).withMultipliedAlpha(value(*this, kOnGlowOpacity));
        canvas.setColor(applause::Brush::radial(glow.withAlpha(0.0f), glow, center, w * 0.5f, h * 0.5f));
        canvas.roundedRectangle(0, 0, w, h, rounding);
        border = color(*this, kOn).withMultipliedAlpha(0.73f + 0.14f * hover);
    }

    if (role_ != Role::Flat) {
        canvas.setColor(border);
        canvas.roundedRectangleBorder(0, 0, w, h, rounding, value(*this, kBorderWidth));
    }

    const float glow = hover * value(*this, kGlowAmount);
    if (glow > 0.0f) {
        canvas.setColor(applause::Brush::radial(color(*this, kGlow).withAlpha(glow), applause::Color(0x00000000),
                                                center, w * 0.5f, h * 0.5f));
        canvas.rectangle(0, 0, w, h);
    }
}

void Button::drawContent(applause::Canvas& canvas, float hover) {
    applause::Color content;
    if (role_ == Role::Primary)
        content = color(*this, kPrimaryText);
    else if (checked_)
        content = color(*this, kOn);
    else if (pressed_)
        content = color(*this, kTextPressed);
    else
        content = color(*this, kText).interpolateWith(color(*this, kTextHover), hover);
    if (!enabled_) content = content.interpolateWith(applause::Color(0xff000000), 0.5f);

    float text_x = 0.0f;
    float text_width = width();
    if (icon_) {
        icon_->setFillBrush(applause::Brush::solid(content));
        icon_->setStrokeBrush(applause::Brush::solid(content));
        text_x = icon_->right() + value(*this, kIconGap);
        text_width = textWidth(text_, dpiScale());
    }

    if (text_.text().isEmpty()) return;
    canvas.setColor(content);
    canvas.text(&text_, text_x, 0, text_width, height());
}

void Button::mouseEnter(const applause::MouseEvent& e) {
    hover_amount_.target(true);
    if (enabled_) applause::setCursorStyle(applause::MouseCursor::Pointing);
    redraw();
}

void Button::mouseExit(const applause::MouseEvent& e) {
    hover_amount_.target(false);
    pressed_ = false;
    applause::setCursorStyle(applause::MouseCursor::Arrow);
    redraw();
}

void Button::mouseDown(const applause::MouseEvent& e) {
    if (!enabled_) return;

    pressed_ = true;
    hover_amount_.target(false);
    redraw();
    if (triggered_on_mouse_down_) click();
}

void Button::mouseUp(const applause::MouseEvent& e) {
    if (!enabled_) return;

    pressed_ = false;
    redraw();
    if (localBounds().contains(e.position)) {
        hover_amount_.target(true, true);
        if (!triggered_on_mouse_down_) click();
    }
}

PopupMenuButton::PopupMenuButton(std::string text) : Button(std::move(text)) {
    onClick() += [this] { showPopup(); };
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
