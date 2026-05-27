#include "InspectorWindow.h"

#ifndef NDEBUG

#include "FrameUtil.h"
#include "InspectorPropertiesView.h"
#include "InspectorThemeView.h"
#include "InspectorTreeView.h"
#include "PickCaptureFrame.h"
#include "SelectionHighlightFrame.h"

#include <cmath>

namespace applause::inspector {

APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorWindow, ApplauseInspectorWindowBackground,  0xff111115);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorWindow, ApplauseInspectorWindowBorder,      0xff2a2a2e);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorWindow, ApplauseInspectorToolbarBackground, 0xff222226);

InspectorWindow::InspectorWindow(applause::Frame& editor) : editor_(editor) {
    setName("InspectorWindow");
    setTitle("Applause Inspector");
    setWindowDecoration(visage::Window::Decoration::Native);
    setAcceptsKeystrokes(true);

    pick_button_      = std::make_unique<applause::ToggleTextButton>("Pick");
    tree_panel_       = std::make_unique<applause::Panel>();
    properties_panel_ = std::make_unique<applause::Panel>();
    theme_panel_      = std::make_unique<applause::Panel>();
    tree_             = std::make_unique<InspectorTreeView>(*this);
    properties_       = std::make_unique<InspectorPropertiesView>(*this);
    theme_            = std::make_unique<InspectorThemeView>(*this);

    addChild(pick_button_.get());
    addChild(tree_panel_.get());
    addChild(properties_panel_.get());
    addChild(theme_panel_.get());
    tree_panel_->content().addChild(tree_.get());
    properties_panel_->content().addChild(properties_.get());
    theme_panel_->content().addChild(theme_.get());

    // Sticky pick: clicking the toggle drives pick mode directly. Clicks on
    // editor widgets while pick mode is on select but don't exit; ESC or
    // re-clicking this toggle exits.
    pick_button_->onToggle() += [this](applause::Button*, bool on) { setPickMode(on); };

    on_pick_mode_changed_ += [this](bool pick) {
        pick_button_->setToggled(pick);
        pick_button_->redraw();
    };

    onCloseRequested() += [this] {
        detachOverlays();
        return true;
    };

    last_frame_count_ = recursiveFrameCount(&editor_);
    startTimer(kPollIntervalMs);
}

InspectorWindow::~InspectorWindow() {
    stopTimer();
    setPickMode(false);  // detach pick_capture_ from editor before teardown
    if (selection_highlight_) {
        editor_.removeChild(selection_highlight_.get());
        selection_highlight_.reset();
    }
    // Detach the palette from the editor so subsequent canvas.color() calls
    // resolve via theme-system defaults again. The palette_ member is about
    // to be destroyed; leaving the editor pointing at it would dangle.
    if (palette_attached_) {
        editor_.setPalette(nullptr);
        palette_attached_ = false;
    }
}

void InspectorWindow::ensurePaletteAttached() {
    if (palette_attached_) return;
    palette_.initWithDefaults();
    editor_.setPalette(&palette_);
    palette_attached_ = true;
}

void InspectorWindow::toggleShown() { setShown(!isShown()); }

bool InspectorWindow::isShown() const { return isShowing(); }

void InspectorWindow::setShown(bool shown) {
    if (shown == isShown()) return;
    if (shown) {
        // Attach the palette to the editor before drawing. initWithDefaults
        // populates every registered theme ID, so the editor's appearance
        // doesn't change — but `canvas.color()` now resolves via the palette,
        // making runtime edits live-update the editor.
        ensurePaletteAttached();
        // First show: rebuild tree and theme list so the user sees something
        // immediately instead of waiting for the polling tick.
        tree_->rebuild();
        theme_->rebuild();
        show(kDefaultWidth, kDefaultHeight);
        refreshSelectionHighlight();
    } else {
        detachOverlays();
        hide();
    }
}

void InspectorWindow::detachOverlays() {
    if (pick_mode_) setPickMode(false);
    if (selection_highlight_) {
        editor_.removeChild(selection_highlight_.get());
        selection_highlight_.reset();
    }
}

void InspectorWindow::setPickMode(bool pick) {
    if (pick_mode_ == pick) return;
    pick_mode_ = pick;

    if (pick) {
        pick_capture_ = std::make_unique<PickCaptureFrame>(*this);
        editor_.addChild(pick_capture_.get());
        pick_capture_->setBounds(0, 0, editor_.width(), editor_.height());
    } else {
        if (pick_capture_) {
            editor_.removeChild(pick_capture_.get());
            pick_capture_.reset();
        }
        setHoveredFrame(nullptr);
    }
    on_pick_mode_changed_.callback(pick);
}

void InspectorWindow::selectFrame(applause::Frame* frame) {
    if (selected_ == frame) return;
    selected_ = frame;
    refreshSelectionHighlight();
    on_selection_changed_.callback(frame);
}

void InspectorWindow::refreshSelectionHighlight() {
    // No highlight when the pop-out is closed or there's no selection (or the
    // selection IS the editor itself — outlining the full editor isn't useful).
    if (!isShown() || selected_ == nullptr || selected_ == &editor_) {
        if (selection_highlight_) {
            editor_.removeChild(selection_highlight_.get());
            selection_highlight_.reset();
        }
        return;
    }

    if (!selection_highlight_) {
        selection_highlight_ = std::make_unique<SelectionHighlightFrame>();
        editor_.addChild(selection_highlight_.get());

        // Drag-to-resize: a handle reports a new selection rect in editor
        // coords; convert into the selected frame's parent coord space, apply,
        // and refresh the highlight synchronously so handles track the cursor
        // at drag-frame rate (the 100 ms polling tick is far too coarse).
        selection_highlight_->setOnResize([this](applause::Bounds new_in_editor) {
            if (!selected_) return;
            applause::Frame* p = selected_->parent();
            const applause::Bounds parent_in_editor =
                (p && p != &editor_)
                    ? editor_.relativeBounds(p)
                    : applause::Bounds(0, 0, editor_.width(), editor_.height());
            selected_->setBounds(
                new_in_editor.x() - parent_in_editor.x(),
                new_in_editor.y() - parent_in_editor.y(),
                new_in_editor.width(),
                new_in_editor.height());
            refreshSelectionHighlight();
        });
    }

    // The highlight frame covers the entire editor so distance-to-parent
    // dashed lines have room to extend past the selection's own bounds.
    selection_highlight_->setBounds(0, 0, editor_.width(), editor_.height());

    const applause::Bounds selected_local = editor_.relativeBounds(selected_);
    applause::Frame* parent = selected_->parent();
    const applause::Bounds parent_local =
        (parent && parent != &editor_)
            ? editor_.relativeBounds(parent)
            : applause::Bounds(0, 0, editor_.width(), editor_.height());

    selection_highlight_->setSelectionBounds(selected_local, parent_local);
}

void InspectorWindow::setHoveredFrame(applause::Frame* frame) {
    if (hovered_ == frame) return;
    hovered_ = frame;
    on_hover_changed_.callback(frame);
}

void InspectorWindow::handlePickHover(applause::Point point) {
    auto* hit = frameAtPointExcluding(&editor_, point, pick_capture_.get());
    setHoveredFrame(hit);
}

void InspectorWindow::handlePickCommit(applause::Point point) {
    // Sticky pick: a click selects the frame under the cursor but does NOT
    // exit pick mode — the user can keep picking until they toggle Pick off
    // or press ESC. pick_capture_ is filtered out of the tree walk by
    // isInternalFrame, so leaving it alive across a rebuild is safe.
    auto* hit = frameAtPointExcluding(&editor_, point, pick_capture_.get());
    if (hit) selectFrame(hit);
}

bool InspectorWindow::isInternalFrame(const applause::Frame* f) const {
    return f != nullptr
        && (f == pick_capture_.get() || f == selection_highlight_.get());
}

void InspectorWindow::draw(applause::Canvas& canvas) {
    canvas.setColor(ApplauseInspectorWindowBackground);
    canvas.rectangle(0, 0, width(), height());

    canvas.setColor(ApplauseInspectorToolbarBackground);
    canvas.rectangle(0, 0, width(), kToolbarHeight);

    canvas.setColor(ApplauseInspectorWindowBorder);
    canvas.rectangle(0, kToolbarHeight, width(), 1.0f);
}

void InspectorWindow::resized() {
    const float w = width();
    const float h = height();
    const float btn_h = kToolbarHeight - 8.0f;
    pick_button_->setBounds(8.0f, 4.0f, w - 16.0f, btn_h);

    const float content_top = kToolbarHeight + 1.0f + kContentPadding;
    const float content_left = kContentPadding;
    const float content_w = w - 2.0f * kContentPadding;
    const float content_h = h - content_top - kContentPadding;

    // Three columns separated by two gaps. Theme column gets the leftover.
    const float column_w = content_w - 2.0f * kColumnGap;
    const float tree_w  = std::floor(column_w * kTreeColumnRatio);
    const float props_w = std::floor(column_w * kPropsColumnRatio);
    const float theme_w = column_w - tree_w - props_w;

    float x = content_left;
    tree_panel_->setBounds(x, content_top, tree_w, content_h);
    x += tree_w + kColumnGap;
    properties_panel_->setBounds(x, content_top, props_w, content_h);
    x += props_w + kColumnGap;
    theme_panel_->setBounds(x, content_top, theme_w, content_h);

    // Tree / properties / theme fill their hosting panel's content frame.
    auto fillContent = [](applause::Panel& panel, applause::Frame& child) {
        child.setBounds(0, 0, panel.content().width(), panel.content().height());
    };
    fillContent(*tree_panel_, *tree_);
    fillContent(*properties_panel_, *properties_);
    fillContent(*theme_panel_, *theme_);

    // Keep the editor-side capture frame full-window during pick mode.
    if (pick_capture_)
        pick_capture_->setBounds(0, 0, editor_.width(), editor_.height());
}

bool InspectorWindow::keyPress(const applause::KeyEvent& e) {
    if (!e.key_down) return false;
    if (e.key_code == applause::KeyCode::Escape && pick_mode_) {
        setPickMode(false);
        return true;
    }
    if (e.key_code == applause::KeyCode::I && e.isMainModifier() && !e.isShiftDown()) {
        toggleShown();
        return true;
    }
    return false;
}

void InspectorWindow::timerCallback() {
    if (!isShown()) return;

    int count = recursiveFrameCount(&editor_);
    if (count != last_frame_count_) {
        last_frame_count_ = count;
        on_tree_changed_.callback();
    }

    // Pick mode: editor may have resized since we attached. Keep the capture
    // frame matching the editor bounds.
    if (pick_capture_) {
        applause::Bounds eb = editor_.localBounds();
        applause::Bounds pb = pick_capture_->bounds();
        if (pb.width() != eb.width() || pb.height() != eb.height())
            pick_capture_->setBounds(0, 0, eb.width(), eb.height());
    }

    // Selection highlight: re-sync selection + parent rects in case the
    // selected frame (or its parent) moved or resized since the last tick.
    // setSelectionBounds is a no-op when nothing changed.
    if (selection_highlight_ && selected_) refreshSelectionHighlight();

    // Properties pane: re-sync editor texts and toggle states with the live
    // frame (skips any editor currently being typed into).
    if (properties_ && selected_) properties_->refreshEditorsFromFrame();
}

}  // namespace applause::inspector

#endif  // NDEBUG
