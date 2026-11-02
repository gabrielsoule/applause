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

#include <applause/ui/PopupMenu.h>

#include <embedded/applause_fonts.h>

namespace applause {
void PopupMenu::show(Frame* source, Point position) {
    std::unique_ptr<PopupMenuFrame> frame = std::make_unique<PopupMenuFrame>(*this);
    PopupMenuFrame* frame_ptr = frame.get();
    frame_ptr->show(std::move(frame), source, position);
}

float PopupList::optionHeight(const PopupMenu& option) const {
    return value(*this, option.isBreak() ? PopupMenu::kSeparatorHeight : PopupMenu::kOptionHeight);
}

float PopupList::renderHeight() const {
    float height = 2.0f * value(*this, PopupMenu::kPadding);
    for (const PopupMenu& option : options_) height += optionHeight(option);
    return height;
}

float PopupList::renderWidth() const {
    float width = value(*this, PopupMenu::kMinWidth);
    float x_padding = value(*this, PopupMenu::kPadding) + value(*this, PopupMenu::kGutter);
    for (const PopupMenu& option : options_) {
        float string_width = font_.stringWidth(option.name().c_str(), option.name().size()) + 2 * x_padding;
        width = std::max(width, string_width);
    }

    return width;
}

int PopupList::yForIndex(int index) const {
    float y = value(*this, PopupMenu::kPadding);
    for (int i = 0; i < index && i < options_.size(); ++i) y += optionHeight(options_[i]);
    return y;
}

void PopupList::selectHoveredIndex() {
    if (hover_index_ >= 0 && hover_index_ < options_.size()) {
        if (options_[hover_index_].hasOptions()) {
            for (Listener* listener : listeners_)
                listener->subMenuSelected(options_[hover_index_], yForIndex(hover_index_), this);
            menu_open_index_ = hover_index_;
        } else {
            for (Listener* listener : listeners_) listener->optionSelected(options_[hover_index_], this);
        }
    }
}

void PopupList::setHoverFromPosition(Point position) {
    float y = value(*this, PopupMenu::kPadding);
    for (int i = 0; i < options_.size(); ++i) {
        float option_height = optionHeight(options_[i]);
        if (!options_[i].isBreak() && options_[i].enabled() && position.y >= y && position.y < y + option_height) {
            hover_index_ = i;
            return;
        }

        y += option_height;
    }

    hover_index_ = -1;
}

void PopupList::selectFromPosition(Point position) {
    setHoverFromPosition(position + Point(0, yPosition()));
    selectHoveredIndex();
}

void PopupList::draw(Canvas& canvas) {
    const float rounding = value(*this, PopupMenu::kRounding);
    const float border_width = value(*this, PopupMenu::kBorderWidth);
    canvas.setColor(color(*this, PopupMenu::kBackground).withMultipliedAlpha(opacity_));
    canvas.roundedRectangle(0, 0, width(), height(), rounding);
    canvas.setColor(color(*this, PopupMenu::kBorder).withMultipliedAlpha(opacity_));
    canvas.roundedRectangleBorder(0, 0, width(), height(), rounding, border_width);

    const float padding = value(*this, PopupMenu::kPadding);
    const float gutter = value(*this, PopupMenu::kGutter);
    const float check_size = value(*this, PopupMenu::kCheckSize);
    const float stroke = check_size * 0.15f;
    const float row_width = width() - 2.0f * padding;
    const Font font = font_.withSize(value(*this, PopupMenu::kFontSize));

    float y = padding - yPosition();
    for (int i = 0; i < options_.size(); ++i) {
        const PopupMenu& option = options_[i];
        const float h = optionHeight(option);
        if (y + h <= 0 || y >= height()) {
            y += h;
            continue;
        }

        if (option.isBreak()) {
            canvas.setColor(color(*this, PopupMenu::kSeparator).withMultipliedAlpha(opacity_));
            canvas.rectangle(2.0f * padding, y + (h - border_width) * 0.5f, width() - 4.0f * padding, border_width);
            y += h;
            continue;
        }

        const bool highlighted = i == hover_index_;
        if (highlighted) {
            const float alpha = value(*this, PopupMenu::kHighlightOpacity) * opacity_;
            canvas.setColor(color(*this, PopupMenu::kHighlight).withMultipliedAlpha(alpha));
            canvas.roundedRectangle(padding, y, row_width, h, value(*this, PopupMenu::kHighlightRounding));
        }

        Color text = color(*this, highlighted ? PopupMenu::kHighlightText : PopupMenu::kText);
        if (!option.enabled()) text = color(*this, PopupMenu::kDisabledText);
        canvas.setColor(text.withMultipliedAlpha(opacity_));
        canvas.text(option.name(), font, Font::kLeft, padding + gutter, y, row_width - 2.0f * gutter, h);

        if (option.selected()) {
            const Color check = color(*this, PopupMenu::kCheck);
            canvas.setColor((option.enabled() ? check : check.interpolateWith(Color(0xff000000), 0.5f))
                                .withMultipliedAlpha(opacity_));
            const float x = padding + (gutter - check_size) * 0.5f;
            const float top = y + (h - check_size) * 0.5f;
            canvas.segment(x + check_size * 0.1f, top + check_size * 0.55f, x + check_size * 0.38f,
                           top + check_size * 0.82f, stroke, true);
            canvas.segment(x + check_size * 0.38f, top + check_size * 0.82f, x + check_size * 0.9f,
                           top + check_size * 0.2f, stroke, true);
        }

        if (option.hasOptions()) {
            canvas.setColor((highlighted ? text : color(*this, PopupMenu::kArrow)).withMultipliedAlpha(opacity_));
            const float arrow = check_size * 0.32f;
            const float x = width() - padding - (gutter + arrow) * 0.5f;
            const float center = y + h * 0.5f;
            canvas.segment(x, center - arrow, x + arrow, center, stroke, true);
            canvas.segment(x + arrow, center, x, center + arrow, stroke, true);
        }
        y += h;
    }
}

void PopupList::resized() {
    ScrollableFrame::resized();
    setScrollableHeight(renderHeight(), height());
}

void PopupList::mouseDown(const MouseEvent& e) {
    Point position = e.relativeTo(this).position;
    if (!isVisible() || !localBounds().contains(position)) return;

    setHoverFromPosition(e.relativeTo(this).position + Point(0, yPosition()));

    if (hover_index_ < options_.size() && hover_index_ >= 0 && options_[hover_index_].hasOptions())
        selectHoveredIndex();

    redraw();
}

void PopupList::mouseExit(const MouseEvent& e) {
    if (!isVisible()) return;

    hover_index_ = menu_open_index_;
    for (Listener* listener : listeners_) listener->mouseMovedOnMenu(e.relativeTo(this).position, this);

    redraw();
}

void PopupList::mouseMove(const MouseEvent& e) {
    if (!isVisible()) return;

    for (Listener* listener : listeners_) listener->mouseMovedOnMenu(e.relativeTo(this).position, this);

    redraw();
}

void PopupList::mouseDrag(const MouseEvent& e) {
    if (!isVisible()) return;

    for (Listener* listener : listeners_) listener->mouseDraggedOnMenu(e.relativeTo(this).position, this);

    redraw();
}

void PopupList::mouseUp(const MouseEvent& e) {
    if (!isVisible()) return;

    Point position = e.relativeTo(this).position;
    if (localBounds().contains(position) == 0) {
        for (Listener* listener : listeners_) listener->mouseUpOutside(position, this);
        return;
    }

    if (enable_mouse_up_) selectFromPosition(position);
    enable_mouse_up_ = true;

    redraw();
}

PopupMenuFrame::PopupMenuFrame(PopupMenu menu) : menu_(std::move(menu)), font_(13, fonts::Barlow_Medium_ttf) {
    opacity_animation_.setTargetValue(1.0f);
    setAcceptsKeystrokes(true);
    setIgnoresMouseEvents(true, true);

    for (auto& list : lists_) {
        addChild(&list);
        list.setVisible(false);
        list.addListener(this);
    }
}

PopupMenuFrame::~PopupMenuFrame() = default;

void PopupMenuFrame::draw(Canvas& canvas) {
    float opacity = opacity_animation_.update();
    for (auto& list : lists_) list.setOpacity(opacity);

    // The lists can't draw outside their bounds, so their shadows are drawn here, behind them.
    const float rounding = value(*this, PopupMenu::kRounding);
    const float shadow = value(*this, PopupMenu::kShadowWidth);
    canvas.setColor(color(*this, PopupMenu::kShadow).withMultipliedAlpha(opacity));
    for (auto& list : lists_) {
        if (list.isVisible())
            canvas.roundedRectangleShadow(list.x(), list.y() + 0.25f * shadow, list.width(), list.height(), rounding,
                                          shadow);
    }

    if (opacity_animation_.isAnimating())
        redraw();
    else if (parent_ && !opacity_animation_.isTargeting())
        exit();
}

void PopupMenuFrame::show(std::unique_ptr<PopupMenuFrame> self, Frame* source, Point point) {
    parent_ = source->topParentFrame();
    parent_->addChild(std::move(self));

    setOnTop(true);
    setBounds(parent_->bounds());

    for (int i = 1; i < kMaxSubMenus; ++i) lists_[i].setVisible(false);

    font_ = font_.withSize(value(*this, PopupMenu::kFontSize));
    setListFonts(font_);

    lists_[0].setOptions(menu_.options());
    int h = std::min(height(), lists_[0].renderHeight());
    int w = lists_[0].renderWidth();

    Bounds window_bounds = parent_->relativeBounds(source);
    int x = point.x == PopupMenu::kNotSet ? window_bounds.x() : window_bounds.x() + point.x;
    int y = point.y == PopupMenu::kNotSet ? window_bounds.bottom() : window_bounds.y() + point.y;
    int bottom = y + h;
    int right = x + w;
    if (bottom > height()) {
        int top = point.y == PopupMenu::kNotSet ? window_bounds.y() : window_bounds.y() + point.y;
        y = std::max(0, top - h);
    }
    if (right > width()) x = std::max(0, x - w);

    for (auto& list : lists_) {
        list.resetOpenMenu();
        list.setNoHover();
    }

    lists_[0].setBounds(x, y, w, h);
    lists_[0].setVisible(true);
    lists_[0].redraw();
    opacity_animation_.target(true, true);

    stopTimer();
    startTimer(kWaitForSelection);
    for (auto& list : lists_) list.enableMouseUp(false);

    requestKeyboardFocus();
    redraw();
}

void PopupMenuFrame::exit() {
    done_ = true;
    startTimer(1);
}

void PopupMenuFrame::hierarchyChanged() {
    if (parent() == nullptr) startTimer(1);
}

void PopupMenuFrame::focusChanged(bool is_focused, bool was_clicked) {
    if (!is_focused && isVisible()) {
        startTimer(1);
        opacity_animation_.target(false);
    }

    redraw();
}

void PopupMenuFrame::timerCallback() {
    if (parent_ && done_) {
        parent_->removeChild(this);
        return;
    }

    redraw();
    stopTimer();

    for (auto& list : lists_) list.enableMouseUp(true);

    if (hover_list_ && hover_index_ >= 0 && hover_index_ < hover_list_->numOptions()) {
        const PopupMenu& option = hover_list_->option(hover_index_);
        if (option.hasOptions()) {
            subMenuSelected(option, hover_list_->hoverY(), hover_list_);
            return;
        }
    }

    int last_open_menu = kMaxSubMenus - 1;
    for (; last_open_menu > 0 && hover_list_ != &lists_[last_open_menu]; --last_open_menu) {
        lists_[last_open_menu].setVisible(false);
        lists_[last_open_menu].resetOpenMenu();
    }

    lists_[last_open_menu].resetOpenMenu();
    if (hover_index_ < 0) lists_[last_open_menu].setNoHover();
}

void PopupMenuFrame::optionSelected(const PopupMenu& option, PopupList* list) {
    if (isVisible()) {
        option.onSelection().callback(option.id());
        menu_.onSelection().callback(option.id());
    } else
        menu_.onCancel().callback();

    exit();
}

void PopupMenuFrame::subMenuSelected(const PopupMenu& option, int selection_y, PopupList* list) {
    int source_index = 0;
    for (int i = 0; i < kMaxSubMenus; ++i) {
        if (list == &lists_[i]) source_index = i;
    }

    lists_[source_index].setOpenMenu(lists_[source_index].hoverIndex());
    if (source_index < kMaxSubMenus - 1) {
        lists_[source_index + 1].setOptions(option.options());
        int h = lists_[source_index + 1].renderHeight();
        int w = lists_[source_index + 1].renderWidth();
        // Lines the submenu's first option up with the option that opened it.
        int y = list->y() + selection_y - value(*this, PopupMenu::kPadding);
        int bottom = y + h;
        int x = lists_[source_index].right();
        int right = x + w;
        if (bottom > height()) y = height() - h;
        if (right > width()) x = lists_[source_index].x() - w;

        lists_[source_index + 1].setBounds(x, y, w, h);
        lists_[source_index + 1].setNoHover();
        lists_[source_index + 1].setVisible(true);
        redraw();
    }
}

void PopupMenuFrame::moveHover(Point position, const PopupList* list) {
    PopupList* last_hover_list = hover_list_;
    int last_hover_index = hover_index_;
    position += list->topLeft();
    hover_list_ = nullptr;
    hover_index_ = -1;
    for (auto& sub_list : lists_) {
        if (sub_list.isVisible() && sub_list.bounds().contains(position)) {
            sub_list.setHoverFromPosition(position - sub_list.topLeft() + Point(0, list->yPosition()));

            hover_list_ = &sub_list;
            hover_index_ = hover_list_->hoverIndex();
        }
    }
    if (hover_list_ != last_hover_list || hover_index_ != last_hover_index) {
        stopTimer();
        startTimer(kPauseMs);
    }
}

void PopupMenuFrame::mouseUpOutside(Point position, PopupList* list) {
    position += list->topLeft();
    for (auto& sub_list : lists_) {
        if (sub_list.isVisible() && sub_list.bounds().contains(position)) {
            sub_list.selectFromPosition(position - sub_list.topLeft());
            return;
        }
    }

    if (isRunning()) return;

    menu_.onCancel().callback();
    exit();
}

void ValueDisplay::showDisplay(const String& text, Bounds bounds, Font::Justification justification) {
    setVisible(true);
    text_ = text;

    Font font = font_.withSize(value(*this, PopupMenu::kFontSize));
    int x_padding = value(*this, PopupMenu::kPadding) + value(*this, PopupMenu::kTextPadding);
    int width = font.stringWidth(text.c_str(), text.length()) + 2 * x_padding;
    int height = value(*this, PopupMenu::kOptionHeight);
    int x = bounds.xCenter() - width / 2;
    int y = bounds.yCenter() - height / 2;
    if (justification & Font::kLeft)
        x = bounds.x() - width;
    else if (justification & Font::kRight)
        x = bounds.right();
    if (justification & Font::kBottom)
        y = bounds.bottom();
    else if (justification & Font::kTop)
        y = bounds.y() - height;

    setBounds(x, y, width, height);
    redraw();
}

void ValueDisplay::draw(Canvas& canvas) {
    Font font = font_.withSize(value(*this, PopupMenu::kFontSize));
    const float rounding = value(*this, PopupMenu::kRounding);
    canvas.setColor(color(*this, PopupMenu::kBackground));
    canvas.roundedRectangle(0, 0, width(), height(), rounding);

    canvas.setColor(color(*this, PopupMenu::kBorder));
    canvas.roundedRectangleBorder(0, 0, width(), height(), rounding, value(*this, PopupMenu::kBorderWidth));

    canvas.setColor(color(*this, PopupMenu::kText));
    canvas.text(text_, font, Font::kCenter, 0, 0, width(), height());
}
}  // namespace applause
