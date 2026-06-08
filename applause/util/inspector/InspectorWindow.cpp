#include "InspectorWindow.h"

#ifndef NDEBUG

#include "FrameUtil.h"
#include "InspectorPropertiesView.h"
#include "InspectorThemeView.h"
#include "InspectorTreeView.h"
#include "PickCaptureFrame.h"
#include "SelectionHighlightFrame.h"

#include <bgfx/bgfx.h>
#include <embedded/applause_fonts.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace applause::inspector {

APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorWindow, ApplauseInspectorWindowBackground,  0xff111115);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorWindow, ApplauseInspectorWindowBorder,      0xff2a2a2e);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorWindow, ApplauseInspectorToolbarBackground, 0xff222226);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorWindow, ApplauseInspectorToolbarText,       0xff9a9aa0);

InspectorWindow::InspectorWindow(applause::Frame& editor)
    : editor_(editor),
      metric_font_(kMetricFontSize, applause::fonts::Jost_Regular_ttf) {
    setName("InspectorWindow");
    setTitle("Applause Inspector");
    setWindowDecoration(visage::Window::Decoration::Native);
    setAcceptsKeystrokes(true);

    pick_button_      = std::make_unique<applause::ToggleTextButton>("Select");
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
}

void InspectorWindow::toggleShown() { setShown(!isShown()); }

bool InspectorWindow::isShown() const { return isShowing(); }

void InspectorWindow::setShown(bool shown) {
    if (shown == isShown()) return;
    if (shown) {
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
        // Keep the selection highlight (and its resize handles) above the
        // pick-capture overlay so handles continue to receive mouse events
        // for dragging while pick mode is active.
        if (selection_highlight_) {
            editor_.removeChild(selection_highlight_.get());
            editor_.addChild(selection_highlight_.get());
        }
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
    auto* hit = frameAtPointExcluding(
        &editor_, point,
        [this](const applause::Frame* f) { return isInternalFrame(f); });
    setHoveredFrame(hit);
}

void InspectorWindow::handlePickCommit(applause::Point point) {
    // Sticky pick: a click selects the frame under the cursor but does NOT
    // exit pick mode — the user can keep picking until they toggle Pick off
    // or press ESC. The predicate filters pick_capture_ and selection_highlight_
    // (and via subtree-skip, the resize handles) out of the walk.
    auto* hit = frameAtPointExcluding(
        &editor_, point,
        [this](const applause::Frame* f) { return isInternalFrame(f); });
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

    // BGFX is initialized lazily by Visage on first window show, so we can
    // only safely flip its profiler flag once we're inside a draw call.
    if (!bgfx_profiler_enabled_) {
        bgfx::setDebug(BGFX_DEBUG_PROFILER);
        bgfx_profiler_enabled_ = true;
    }

    // First few frames give bogus dt (deltaTime() is 0 until a previous frame
    // exists). Ignore samples outside a plausible 1 Hz – 1000 Hz range so the
    // EMA doesn't seed with a junk value it then takes seconds to drift off of.
    const double dt = canvas.deltaTime();
    if (dt >= 0.001 && dt <= 1.0) {
        const float inst_fps = float(1.0 / dt);
        smoothed_fps_ = smoothed_fps_ == 0.0f
            ? inst_fps
            : smoothed_fps_ + kMetricSmoothing * (inst_fps - smoothed_fps_);
    }

    const bgfx::Stats* s = bgfx::getStats();
    // cpuTimeBegin/End bracket the render-thread submit work and exclude the
    // vsync wait, unlike cpuTimeFrame (which is wall-clock between frame()
    // calls and just reports the budget back at you).
    const float inst_cpu_ms = s->cpuTimerFreq > 0
        ? float(1000.0 * double(s->cpuTimeEnd - s->cpuTimeBegin) / double(s->cpuTimerFreq)) : 0.0f;
    const float inst_gpu_ms = s->gpuTimerFreq > 0
        ? float(1000.0 * double(s->gpuTimeEnd - s->gpuTimeBegin) / double(s->gpuTimerFreq)) : 0.0f;
    smoothed_cpu_ms_ += kMetricSmoothing * (inst_cpu_ms - smoothed_cpu_ms_);
    smoothed_gpu_ms_ += kMetricSmoothing * (inst_gpu_ms - smoothed_gpu_ms_);

    // numDraw / numPrims are written by the BGFX render thread; with Visage's
    // single-threaded multi-window setup they don't propagate reliably here.
    // We surface fields that getPerfStats() fills synchronously instead.
    const float tex_mb       = float(s->textureMemoryUsed) / (1024.0f * 1024.0f);
    const int   fb_count     = int(s->numFrameBuffers);
    const int   tex_count    = int(s->numTextures);
    const int   frame_count  = last_frame_count_;

    const int cpu_pct = int(std::round(100.0f * smoothed_cpu_ms_ / kFrameBudgetMs));

    char buf[7][32];
    std::snprintf(buf[0], sizeof(buf[0]), "%.1f fps", smoothed_fps_);
    std::snprintf(buf[1], sizeof(buf[1]), "%.1f / %.1f ms (%d%%)",
                  smoothed_cpu_ms_, kFrameBudgetMs, cpu_pct);
    if (smoothed_gpu_ms_ > 0.005f)
        std::snprintf(buf[2], sizeof(buf[2]), "%.1f gpu", smoothed_gpu_ms_);
    else
        std::snprintf(buf[2], sizeof(buf[2]), "— gpu");
    std::snprintf(buf[3], sizeof(buf[3]), "%d fbs", fb_count);
    std::snprintf(buf[4], sizeof(buf[4]), "%d tex", tex_count);
    std::snprintf(buf[5], sizeof(buf[5]), "%.1f MB tex", tex_mb);
    std::snprintf(buf[6], sizeof(buf[6]), "%d frames", frame_count);

    const float slot_widths[7] = {
        kMetricSlotWidth,  // fps
        kCpuSlotWidth,     // cpu / budget (%)
        kMetricSlotWidth,  // gpu
        kMetricSlotWidth,  // fbs
        kMetricSlotWidth,  // tex
        kMetricSlotWidth,  // tex MB
        kMetricSlotWidth,  // frames
    };

    canvas.setColor(ApplauseInspectorToolbarText);
    float x = 6.0f + kPickButtonWidth + 10.0f;
    for (int i = 0; i < 7; ++i) {
        canvas.text(applause::String(buf[i]), metric_font_, applause::Font::kLeft,
                    x, 0.0f, slot_widths[i], kToolbarHeight);
        x += slot_widths[i];
    }

    canvas.setColor(ApplauseInspectorWindowBorder);
    canvas.rectangle(0, kToolbarHeight, width(), 1.0f);
}

void InspectorWindow::resized() {
    const float w = width();
    const float h = height();
    const float btn_h = kToolbarHeight - 8.0f;
    pick_button_->setBounds(6.0f, 4.0f, kPickButtonWidth, btn_h);

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

    // Toolbar metrics (FPS / draws / mem / ...) update inside draw(), so force
    // a repaint each tick to keep the numbers live even when nothing else is
    // animating.
    redraw();
}

}  // namespace applause::inspector

#endif  // NDEBUG
