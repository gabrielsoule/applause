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

#include <applause/ui/Frame.h>

namespace applause {
void Frame::setVisible(bool visible) {
    if (visible_ != visible) {
        visible_ = visible;
        on_visibility_change_.callback();
    }

    region_.setVisible(visible);
    if (group_) group_->setVisible(visible);
    if (visible)
        redraw();
    else
        redrawing_ = false;

    setDrawing(visible && (parent_ == nullptr || parent_->isDrawing()));

    if (!visible && keyboardFocusInside() && event_handler_->request_keyboard_focus)
        event_handler_->request_keyboard_focus(nullptr);
}

void Frame::setDrawing(bool drawing) {
    if (drawing == drawing_) return;

    drawing_ = drawing;
    if (drawing_)
        redraw();
    else {
        redrawing_ = false;
        region_.setNeedsLayer(false);
        if (group_) group_->setNeedsLayer(false);
    }

    for (Frame* child : children_) {
        if (child->isVisible() && child->isDrawing() != drawing_) child->setDrawing(drawing_);
    }
}

void Frame::setOnTop(bool on_top) {
    on_top_ = on_top;
    outerRegion()->setOnTop(on_top);
    redraw();
}

void Frame::addChild(Frame* child, bool make_visible) {
    APPLAUSE_ASSERT(child && child != this);
    if (child == nullptr) return;

    if (child->parent_) {
        auto owned = child->parent_->owned_children_.extract(child);
        child->parent_->removeChild(child);
        if (owned) owned_children_[child] = std::move(owned.mapped());
    }

    children_.push_back(child);
    child->parent_ = this;
    child->setEventHandler(event_handler_);

    if (!make_visible) child->setVisible(false);
    child->setDrawing(child->isVisible() && isDrawing());

    region_.addRegion(child->outerRegion());

    child->setDpiScale(childDpiScale(child));
    if (initialized_) child->init();

    child->notifyHierarchyChanged();
    on_child_added_.callback(child);
    computeLayout();
    computeLayout(child);
    child->redrawAll();
}

void Frame::addChild(std::unique_ptr<Frame> child, bool make_visible) {
    addChild(child.get(), make_visible);
    owned_children_[child.get()] = std::move(child);
}

void Frame::removeChild(Frame* child) {
    APPLAUSE_ASSERT(child && child != this);
    if (child == nullptr || child->parent_ != this) return;

    Frame* focused = child->keyboardFocusInside();
    child->notifyRemoveFromHierarchy();
    eraseChild(child);
    child->notifyHierarchyChanged();
    on_child_removed_.callback(child);

    auto owned = owned_children_.extract(child);
    computeLayout();
    if (focused && (focused->hasKeyboardFocus() || !focused->acceptsKeystrokes()))
        focused->processFocusChanged(false, false);
}

void Frame::detachAllChildren() {
    while (!children_.empty()) {
        children_.back()->notifyRemoveFromHierarchy();
        eraseChild(children_.back());
    }

    owned_children_.clear();
}

int Frame::indexOfChild(const Frame* child) const {
    for (int i = 0; i < children_.size(); ++i) {
        if (children_[i] == child) return i;
    }
    return -1;
}

Frame* Frame::frameAtPoint(Point point) {
    if (pass_mouse_events_to_children_) {
        point = point / child_scale_;
        for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
            auto& child = *it;
            if (child->isOnTop() && child->isVisible() && child->containsPoint(point)) {
                Frame* result = child->frameAtPoint(point - child->topLeft());
                if (result) return result;
            }
        }
        for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
            auto& child = *it;
            if (!child->isOnTop() && child->isVisible() && child->containsPoint(point)) {
                Frame* result = child->frameAtPoint(point - child->topLeft());
                if (result) return result;
            }
        }
    }

    if (!ignores_mouse_events_) return this;
    return nullptr;
}

Frame* Frame::topParentFrame() {
    Frame* frame = this;
    while (frame->parent_) frame = frame->parent_;
    return frame;
}

void Frame::setBounds(Bounds bounds) {
    IBounds new_native_bounds = (bounds * dpi_scale_).round();
    if (bounds_ == bounds && native_bounds_ == new_native_bounds) return;

    bounds_ = bounds;
    native_bounds_ = new_native_bounds;
    outerRegion()->setBounds(native_bounds_.x(), native_bounds_.y(), native_bounds_.width(), native_bounds_.height());
    if (group_) region_.setBounds(0, 0, native_bounds_.width(), native_bounds_.height());
    redraw();
    runLayout();
    if (parent_) parent_->on_child_bounds_changed_.callback(this);
}

void Frame::runLayout() {
    computeLayout();
    if (layout_ == nullptr || !layout_->flex()) {
        for (Frame* child : children_) computeLayout(child);
    }

    if (mask_frame_) mask_frame_->setBounds(localBounds());

    on_resize_.callback();
}

void Frame::runLayoutRecursively() {
    runLayout();
    redraw();
    // Copied, since laying a frame out may add or remove its children.
    const std::vector<Frame*> children = children_;
    for (Frame* child : children) child->runLayoutRecursively();
}

void Frame::setNativeBounds(IBounds native_bounds) { setBounds(Bounds(native_bounds) * (1.0f / dpi_scale_)); }

IBounds Frame::computeLayoutBoundingBox(IBounds bounds) const {
    if (bounds.width() == 0 || bounds.height() == 0 || layout_ == nullptr || !layout_->flex()) return {};

    std::vector<const Layout*> children_layouts;
    for (Frame* child : children_) {
        if (child->layout_) children_layouts.push_back(child->layout_.get());
    }
    layout_->flexPositions(children_layouts, bounds, dpi_scale_ * child_scale_);
    return layout_->boundingBox();
}

void Frame::computeLayout() {
    if (nativeWidth() && nativeHeight() && layout_.get() && layout_->flex()) {
        std::vector<const Layout*> children_layouts;
        for (Frame* child : children_) {
            if (child->layout_) children_layouts.push_back(child->layout_.get());
        }

        std::vector<IBounds> children_bounds =
            layout_->flexPositions(children_layouts, nativeLocalBounds(), dpi_scale_ * child_scale_);
        int index = 0;
        for (Frame* child : children_) {
            if (child->layout_) child->setNativeBounds(children_bounds[index++]);
        }
    }
}

void Frame::computeLayout(Frame* child) {
    if (child->layout_ == nullptr || (layout_ && layout_->flex())) return;

    int width = this->nativeWidth();
    int height = this->nativeHeight();
    float dpi = child->dpi_scale_;

    int pad_left = 0;
    int pad_top = 0;
    int pad_right = 0;
    int pad_bottom = 0;

    if (layout_) {
        pad_left = layout_->paddingLeft().computeInt(dpi, width, height, 0);
        pad_top = layout_->paddingTop().computeInt(dpi, width, height, 0);
        pad_right = layout_->paddingRight().computeInt(dpi, width, height, 0);
        pad_bottom = layout_->paddingBottom().computeInt(dpi, width, height, 0);
    }

    int x = child->nativeX();
    int y = child->nativeY();
    int dist_right = width - child->nativeRight();
    int dist_bottom = height - child->nativeBottom();

    x = pad_left + child->layout_->marginLeft().computeInt(dpi, width, height, x - pad_left);
    y = pad_top + child->layout_->marginTop().computeInt(dpi, width, height, y - pad_top);
    dist_right = pad_right + child->layout_->marginRight().computeInt(dpi, width, height, dist_right - pad_right);
    dist_bottom = pad_bottom + child->layout_->marginBottom().computeInt(dpi, width, height, dist_bottom - pad_bottom);

    int right = width - dist_right;
    int bottom = height - dist_bottom;
    int w = child->layout_->width().computeInt(dpi, width, height, right - x);
    int h = child->layout_->height().computeInt(dpi, width, height, bottom - y);
    child->setNativeBounds(x, y, w, h);
}

void Frame::setChildScale(float scale) {
    APPLAUSE_ASSERT(scale > 0.0f);
    if (child_scale_ == scale) return;

    child_scale_ = scale;
    for (Frame* child : children_) child->setDpiScale(childDpiScale(child));
    runLayout();
}

Point Frame::positionInWindow() const {
    Point global_position = topLeft();
    Frame* frame = parent_;
    while (frame) {
        global_position = global_position * frame->child_scale_ + frame->topLeft();
        frame = frame->parent_;
    }

    return global_position;
}

float Frame::scaleInWindow() const {
    float scale = 1.0f;
    for (Frame* frame = parent_; frame; frame = frame->parent_) scale *= frame->child_scale_;
    return scale;
}

Point Frame::windowToLocal(Point window_position) const {
    return (window_position - positionInWindow()) / scaleInWindow();
}

Bounds Frame::relativeBounds(const Frame* other) const {
    Point position = windowToLocal(other->positionInWindow());
    float scale = other->scaleInWindow() / scaleInWindow();
    return {position.x, position.y, other->bounds().width() * scale, other->bounds().height() * scale};
}

bool Frame::tryFocusTextReceiver() {
    if (!isVisible()) return false;

    if (receivesTextInput()) {
        requestKeyboardFocus();
        return true;
    }

    for (auto& child : children_) {
        if (child->tryFocusTextReceiver()) return true;
    }
    return false;
}

bool Frame::focusNextTextReceiver(const Frame* starting_child) const {
    int index = indexOfChild(starting_child);
    for (int i = index + 1; i < children_.size(); ++i) {
        if (children_[i]->tryFocusTextReceiver()) return true;
    }

    if (parent_ && parent_->focusNextTextReceiver(this)) return true;

    for (int i = 0; i < index; ++i) {
        if (children_[i]->tryFocusTextReceiver()) return true;
    }
    return false;
}

bool Frame::focusPreviousTextReceiver(const Frame* starting_child) const {
    int index = indexOfChild(starting_child);
    for (int i = index - 1; i >= 0; --i) {
        if (children_[i]->tryFocusTextReceiver()) return true;
    }

    if (parent_ && parent_->focusPreviousTextReceiver(this)) return true;

    for (int i = children_.size() - 1; i > index; --i) {
        if (children_[i]->tryFocusTextReceiver()) return true;
    }
    return false;
}

void Frame::initChildren() {
    if (initialized_) return;

    initialized_ = true;
    for (Frame* child : children_) child->init();
}

void Frame::drawToRegion(Canvas& canvas) {
    if (!redrawing_) return;

    redrawing_ = false;
    region_.invalidate();
    if (needsGroup() != grouped_) updateGroup();
    if (group_) group_->setNeedsLayer(grouped_);
    region_.setNeedsLayer(requiresLayer());
    if (width() <= 0 || height() <= 0) {
        region_.clear();
        return;
    }

    canvas.beginRegion(&region_, dpi_scale_);
    on_draw_.callback(canvas);
    canvas.endRegion();
}

void Frame::destroyChildren() {
    initialized_ = false;
    for (Frame* child : children_) child->destroy();
}

void Frame::eraseChild(Frame* child) {
    child->parent_ = nullptr;
    child->setEventHandler(nullptr);
    if (Region* parent = child->outerRegion()->parent()) parent->removeRegion(child->outerRegion());
    children_.erase(std::find(children_.begin(), children_.end(), child));
}

void Frame::createMaskFrame() {
    mask_frame_ = std::make_unique<Frame>("mask");
    mask_frame_->setLayerBlendMode(BlendMode::MaskMult);
    mask_frame_->setOnTop(true);
    mask_frame_->setIgnoresMouseEvents(true, false);
    addChild(mask_frame_.get());
    region_.removeRegion(mask_frame_->region());

    updateGroup();
    group_->addRegion(mask_frame_->region());
    mask_frame_->setBounds(localBounds());
    setMaskType(mask_type_);
    redraw();
}

void Frame::updateGroup() {
    grouped_ = needsGroup();
    if (grouped_ && group_ == nullptr) {
        group_ = std::make_unique<Region>();
        if (Region* parent = region_.parent()) parent->replaceRegion(&region_, group_.get());
        group_->addRegion(&region_);
        group_->setVisible(visible_);
        group_->setOnTop(on_top_);
        group_->setBounds(native_bounds_.x(), native_bounds_.y(), native_bounds_.width(), native_bounds_.height());
        region_.setOnTop(false);
        region_.setBounds(0, 0, native_bounds_.width(), native_bounds_.height());
    }

    Region* composite = grouped_ ? group_.get() : &region_;
    composite->setAlpha(alpha_transparency_);
    composite->setLayerBlendMode(layer_blend_mode_);
    if (group_) {
        Region* inner = grouped_ ? &region_ : group_.get();
        inner->setAlpha(1.0f);
        inner->setLayerBlendMode(BlendMode::Alpha);
        group_->setMaskedRegion(grouped_ ? &region_ : nullptr);
    }
}

void Frame::setMaskType(MaskType mask_type) {
    mask_type_ = mask_type;
    if (mask_frame_ == nullptr) return;

    if (mask_type_ == MaskType::Luminance) {
        if (luminance_mask_effect_ == nullptr) luminance_mask_effect_ = std::make_unique<LuminanceMaskPostEffect>();
        mask_frame_->setPostEffect(luminance_mask_effect_.get());
    } else if (mask_frame_->postEffect())
        mask_frame_->removePostEffect();
}

void Frame::setPostEffect(PostEffect* post_effect) {
    region_.setPostEffect(post_effect);
    updateGroup();
    if (parent_) parent_->redraw();
    redrawAll();
}

void Frame::removePostEffect() {
    APPLAUSE_ASSERT(postEffect());
    region_.setPostEffect(nullptr);
    updateGroup();
    redrawAll();
}

void Frame::setBackdropEffect(PostEffect* backdrop_effect) {
    region_.setBackdropEffect(backdrop_effect);
    if (parent_) parent_->redraw();
    redrawAll();
}

void Frame::removeBackdropEffect() {
    APPLAUSE_ASSERT(backdropEffect());
    region_.setBackdropEffect(nullptr);
    redrawAll();
}

// Colors are only read while drawing, so a color change redraws. Values can also be read during layout, so a value
// change lays out again.
void Frame::set(const ColorToken& token, Color color) {
    theme_colors_[&token] = color;
    redrawAll();
}

void Frame::set(const ValueToken& token, float value) {
    theme_values_[&token] = value;
    runLayoutRecursively();
}

void Frame::set(const ColorToken& token, const ColorToken& source) {
    APPLAUSE_ASSERT(&source != &token);
    theme_colors_[&token] = &source;
    redrawAll();
}

void Frame::set(const ValueToken& token, const ValueToken& source) {
    APPLAUSE_ASSERT(&source != &token);
    theme_values_[&token] = &source;
    runLayoutRecursively();
}

void Frame::unset(const ColorToken& token) {
    if (theme_colors_.erase(&token)) redrawAll();
}

void Frame::unset(const ValueToken& token) {
    if (theme_values_.erase(&token)) runLayoutRecursively();
}

const Setting<Color>* Frame::find(const ColorToken& token) const {
    auto it = theme_colors_.find(&token);
    return it == theme_colors_.end() ? nullptr : &it->second;
}

const Setting<float>* Frame::find(const ValueToken& token) const {
    auto it = theme_values_.find(&token);
    return it == theme_values_.end() ? nullptr : &it->second;
}

template <typename T>
T Token<T>::resolve(const Frame& frame) const {
    for (const Frame* f = &frame; f; f = f->parent()) {
        if (const Setting<T>* setting = f->find(*this)) {
            if (const T* value = std::get_if<T>(setting)) return *value;
            return std::get<const Token*>(*setting)->resolve(frame);
        }
    }
    return base_ ? base_->resolve(frame) : value_;
}

Frame* Frame::keyboardFocusInside() const {
    if (event_handler_ == nullptr || event_handler_->keyboard_focused_frame == nullptr) return nullptr;

    Frame* focused = event_handler_->keyboard_focused_frame();
    for (const Frame* frame = focused; frame; frame = frame->parent_) {
        if (frame == this) return focused;
    }
    return nullptr;
}

UndoHistory* Frame::undoHistory() const {
    if (auto history = dynamic_cast<UndoHistory*>(const_cast<Frame*>(this))) return history;
    return findParent<UndoHistory>();
}

template <typename T>
std::vector<const Token<T>*>& Token<T>::all() {
    static std::vector<const Token*> tokens;
    return tokens;
}

template class Token<Color>;
template class Token<float>;

Color color(const Frame& frame, const ColorToken& token) { return token.resolve(frame); }

float value(const Frame& frame, const ValueToken& token) { return token.resolve(frame); }

void Frame::addUndoableAction(std::unique_ptr<UndoableAction> action) const {
    UndoHistory* history = undoHistory();
    if (history) history->push(std::move(action));
}

void Frame::triggerUndo() const {
    UndoHistory* history = undoHistory();
    if (history) history->undo();
}

void Frame::triggerRedo() const {
    UndoHistory* history = undoHistory();
    if (history) history->redo();
}

bool Frame::canUndo() const {
    UndoHistory* history = undoHistory();
    if (history) return history->canUndo();
    return false;
}

bool Frame::canRedo() const {
    UndoHistory* history = undoHistory();
    if (history) return history->canRedo();
    return false;
}
}  // namespace applause
