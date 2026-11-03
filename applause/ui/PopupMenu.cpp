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
    if (!isVisible() || e.isCancelled()) return;

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
    popupList(0);
}

PopupMenuFrame::~PopupMenuFrame() = default;

PopupList& PopupMenuFrame::popupList(int index) {
    while (lists_.size() <= index) {
        auto list = std::make_unique<PopupList>();
        addChild(list.get(), false);
        list->addListener(this);
        list->setFont(font_);
        list->setOpacity(opacity_animation_.value());
        list->enableMouseUp(mouse_up_enabled_);
        lists_.push_back(std::move(list));
    }
    return *lists_[index];
}

Frame* PopupMenuFrame::keyboardFocusedFrame() const {
    FrameEventHandler* handler = eventHandler();
    return handler && handler->keyboard_focused_frame ? handler->keyboard_focused_frame() : nullptr;
}

int PopupMenuFrame::listIndex(const PopupList* list) const {
    for (int i = 0; i < lists_.size(); ++i) {
        if (lists_[i].get() == list) return i;
    }
    return 0;
}

void PopupMenuFrame::draw(Canvas& canvas) {
    float opacity = opacity_animation_.update();
    for (auto& list : lists_) list->setOpacity(opacity);

    // The lists can't draw outside their bounds, so their shadows are drawn here, behind them.
    const float rounding = value(*this, PopupMenu::kRounding);
    const float shadow = value(*this, PopupMenu::kShadowWidth);
    canvas.setColor(color(*this, PopupMenu::kShadow).withMultipliedAlpha(opacity));
    for (auto& list : lists_) {
        if (list->isVisible())
            canvas.roundedRectangleShadow(list->x(), list->y() + 0.25f * shadow, list->width(), list->height(),
                                          rounding, shadow);
    }

    if (opacity_animation_.isAnimating())
        redraw();
    else if (top_parent_ && !opacity_animation_.isTargeting())
        exit();
}

void PopupMenuFrame::show(std::unique_ptr<PopupMenuFrame> self, Frame* source, Point point) {
    top_parent_ = source->topParentFrame();
    top_parent_->addChild(std::move(self));

    // The menu sits on the top frame, so it copies the theme set on the frames between there and the source.
    for (const ColorToken* token : ColorToken::all()) {
        Color source_color = color(*source, *token);
        if (color(*this, *token) != source_color) set(*token, source_color);
    }
    for (const ValueToken* token : ValueToken::all()) {
        float source_value = value(*source, *token);
        if (value(*this, *token) != source_value) set(*token, source_value);
    }

    setOnTop(true);
    setBounds(top_parent_->bounds());

    for (int i = 1; i < lists_.size(); ++i) lists_[i]->setVisible(false);

    font_ = font_.withSize(value(*this, PopupMenu::kFontSize));
    setListFonts(font_);

    PopupList& main_list = popupList(0);
    main_list.setOptions(menu_.options());
    int h = std::min(height(), main_list.renderHeight());
    int w = main_list.renderWidth();

    Bounds window_bounds = top_parent_->relativeBounds(source);
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
        list->resetOpenMenu();
        list->setNoHover();
    }

    main_list.setBounds(x, y, w, h);
    main_list.setVisible(true);
    main_list.redraw();
    opacity_animation_.target(true, true);

    mouse_up_enabled_ = false;
    for (auto& list : lists_) list->enableMouseUp(false);

    stopTimer();
    startTimer(takeMouseDrag() ? kPauseMs : kWaitForSelection);

    previous_focus_ = keyboardFocusedFrame();
    if (previous_focus_) previous_focus_alive_ = previous_focus_->aliveToken();
    requestKeyboardFocus();
    redraw();
}

void PopupMenuFrame::mouseDrag(const MouseEvent& e) {
    PopupList& main_list = popupList(0);
    moveHover(e.position - main_list.topLeft(), &main_list);
    dragged_onto_menu_ = dragged_onto_menu_ || hover_list_ != nullptr;
}

void PopupMenuFrame::mouseUp(const MouseEvent& e) {
    bool over_menu = false;
    for (auto& list : lists_) over_menu = over_menu || (list->isVisible() && list->bounds().contains(e.position));

    if (!e.isCancelled() && dragged_onto_menu_ && (mouse_up_enabled_ || !over_menu)) {
        PopupList& main_list = popupList(0);
        mouseUpOutside(e.position - main_list.topLeft(), &main_list);
    }

    dragged_onto_menu_ = false;
    mouse_up_enabled_ = true;
    for (auto& list : lists_) list->enableMouseUp(true);
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
        if (!done_ && opacity_animation_.isTargeting()) menu_.onCancel().callback();

        startTimer(1);
        opacity_animation_.target(false);
    }

    redraw();
}

void PopupMenuFrame::timerCallback() {
    if (top_parent_ && done_) {
        if (keyboardFocusedFrame() == this && !previous_focus_alive_.expired() && previous_focus_->isDrawing())
            previous_focus_->requestKeyboardFocus();
        top_parent_->removeChild(this);
        return;
    }

    redraw();
    stopTimer();

    mouse_up_enabled_ = true;
    for (auto& list : lists_) list->enableMouseUp(true);

    if (hover_list_ && hover_index_ >= 0 && hover_index_ < hover_list_->numOptions()) {
        const PopupMenu& option = hover_list_->option(hover_index_);
        if (option.hasOptions()) {
            subMenuSelected(option, hover_list_->hoverY(), hover_list_);
            return;
        }
    }

    int last_open_menu = lists_.size() - 1;
    for (; last_open_menu > 0 && hover_list_ != lists_[last_open_menu].get(); --last_open_menu) {
        lists_[last_open_menu]->setVisible(false);
        lists_[last_open_menu]->resetOpenMenu();
    }

    lists_[last_open_menu]->resetOpenMenu();
    if (hover_index_ < 0) lists_[last_open_menu]->setNoHover();
}

void PopupMenuFrame::optionSelected(const PopupMenu& option, PopupList* list) {
    if (done_) return;

    if (isVisible()) {
        option.onSelection().callback(option.id());
        menu_.onSelection().callback(option.id());
    } else
        menu_.onCancel().callback();

    exit();
}

void PopupMenuFrame::subMenuSelected(const PopupMenu& option, int selection_y, PopupList* list) {
    int source_index = listIndex(list);
    PopupList& source = *lists_[source_index];
    PopupList& sub_list = popupList(source_index + 1);

    bool reopening = source.openMenuIndex() == source.hoverIndex() && sub_list.isVisible();
    source.setOpenMenu(source.hoverIndex());
    sub_list.setOptions(option.options());
    int h = std::min(height(), sub_list.renderHeight());
    int w = sub_list.renderWidth();
    // Lines the submenu's first option up with the option that opened it.
    int y = list->y() + selection_y - list->yPosition() - value(*this, PopupMenu::kPadding);
    int bottom = y + h;
    int x = source.right();
    int right = x + w;
    if (bottom > height()) y = height() - h;
    y = std::max(0, y);
    if (right > width()) x = source.x() - w;

    sub_list.setBounds(x, y, w, h);
    if (!reopening) {
        sub_list.setYPosition(0);
        sub_list.setScrollableHeight(sub_list.renderHeight(), h);
    }
    sub_list.setNoHover();
    sub_list.setVisible(true);
    redraw();
}

void PopupMenuFrame::moveHover(Point position, const PopupList* list) {
    PopupList* last_hover_list = hover_list_;
    int last_hover_index = hover_index_;
    position += list->topLeft();
    hover_list_ = nullptr;
    hover_index_ = -1;
    for (auto& entry : lists_) {
        PopupList& sub_list = *entry;
        if (sub_list.isVisible() && sub_list.bounds().contains(position)) {
            sub_list.setHoverFromPosition(position - sub_list.topLeft() + Point(0, sub_list.yPosition()));
            sub_list.redraw();

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
    if (done_) return;

    position += list->topLeft();
    for (auto& entry : lists_) {
        PopupList& sub_list = *entry;
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
