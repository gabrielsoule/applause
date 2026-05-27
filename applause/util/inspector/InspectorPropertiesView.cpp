#include "InspectorPropertiesView.h"

#ifndef NDEBUG

#include "FrameUtil.h"
#include "InspectorWindow.h"

#include <embedded/applause_fonts.h>

#include <cstdio>
#include <cstdlib>

namespace applause::inspector {

APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorPropertiesView, ApplauseInspectorPropertiesKey,   0xff7a7a80);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorPropertiesView, ApplauseInspectorPropertiesValue, 0xffd8d8dc);

namespace {

constexpr int kRowType         = 0;
constexpr int kRowName         = 1;
constexpr int kRowBounds       = 2;
constexpr int kRowVisible      = 3;
constexpr int kRowOnTop        = 4;
constexpr int kRowIgnoresMouse = 5;
constexpr int kRowParent       = 6;
constexpr int kRowChildren     = 7;
constexpr int kRowSubtreeSize  = 8;

void configureNumberEditor(applause::TextEditor& e) {
    e.setNumberEntry();
    // NOTE: visage's setFilteredCharacters() is a BLACKLIST (chars to strip),
    // not a whitelist. There's no "allow only digits" knob, so we validate at
    // commit time instead (std::strtof falls back if parsing fails).
    e.setMaxCharacters(8);
    e.setFont(applause::Font(InspectorPropertiesView::kFontSize,
                             applause::fonts::Jost_Regular_ttf));
}

}  // namespace

InspectorPropertiesView::InspectorPropertiesView(InspectorWindow& window)
    : window_(window),
      label_font_(kFontSize, applause::fonts::Jost_Regular_ttf),
      x_editor_(std::make_unique<applause::TextEditor>("inspector_x")),
      y_editor_(std::make_unique<applause::TextEditor>("inspector_y")),
      w_editor_(std::make_unique<applause::TextEditor>("inspector_w")),
      h_editor_(std::make_unique<applause::TextEditor>("inspector_h")),
      visible_toggle_(std::make_unique<applause::ToggleTextButton>("inspector_visible")),
      on_top_toggle_(std::make_unique<applause::ToggleTextButton>("inspector_on_top")),
      ignores_mouse_toggle_(std::make_unique<applause::ToggleTextButton>("inspector_ignores_mouse")) {

    applause::TextEditor* editors[] = {x_editor_.get(), y_editor_.get(),
                                       w_editor_.get(), h_editor_.get()};
    for (auto* e : editors) {
        configureNumberEditor(*e);
        addChild(e);
        e->setVisible(false);
        e->onEnterKey()  += [this]() { commitBoundsFromEditors(); };
        e->onEscapeKey() += [this]() { refreshEditorsFromFrame(); };
        // NB: visage's on_focus_change_ callback list is declared but never
        // invoked by the framework, so commit-on-blur isn't possible without
        // subclassing. The user must press Enter to commit; the 100 ms
        // refresh tick skips focused editors so typing isn't stomped.
    }

    applause::ToggleTextButton* toggles[] = {visible_toggle_.get(),
                                             on_top_toggle_.get(),
                                             ignores_mouse_toggle_.get()};
    for (auto* t : toggles) {
        t->setFont(applause::Font(kFontSize, applause::fonts::Jost_Regular_ttf));
        addChild(t);
        t->setVisible(false);
    }

    visible_toggle_->onToggle() += [this](applause::Button*, bool on) {
        if (suppress_toggle_callback_) return;
        if (auto* sel = window_.selectedFrame()) sel->setVisible(on);
        visible_toggle_->setText(on ? "on" : "off");
    };
    on_top_toggle_->onToggle() += [this](applause::Button*, bool on) {
        if (suppress_toggle_callback_) return;
        if (auto* sel = window_.selectedFrame()) sel->setOnTop(on);
        on_top_toggle_->setText(on ? "on" : "off");
    };
    ignores_mouse_toggle_->onToggle() += [this](applause::Button*, bool on) {
        if (suppress_toggle_callback_) return;
        if (auto* sel = window_.selectedFrame())
            sel->setIgnoresMouseEvents(on, true);
        ignores_mouse_toggle_->setText(on ? "on" : "off");
    };

    window_.onSelectionChanged() += [this](applause::Frame*) {
        refreshEditorsFromFrame();
        redraw();
    };
}

InspectorPropertiesView::~InspectorPropertiesView() = default;

static std::string formatNumber(float v) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.0f", v);
    return buf;
}

void InspectorPropertiesView::refreshEditorsFromFrame() {
    auto* sel = window_.selectedFrame();
    const bool show = sel != nullptr;

    applause::TextEditor* editors[] = {x_editor_.get(), y_editor_.get(),
                                       w_editor_.get(), h_editor_.get()};
    applause::ToggleTextButton* toggles[] = {visible_toggle_.get(),
                                             on_top_toggle_.get(),
                                             ignores_mouse_toggle_.get()};
    for (auto* e : editors) e->setVisible(show);
    for (auto* t : toggles) t->setVisible(show);

    if (!show) {
        redraw();
        return;
    }

    auto setIfUnfocused = [](applause::TextEditor* e, float v) {
        if (e->hasKeyboardFocus()) return;
        e->setText(formatNumber(v));
    };
    setIfUnfocused(x_editor_.get(), sel->x());
    setIfUnfocused(y_editor_.get(), sel->y());
    setIfUnfocused(w_editor_.get(), sel->width());
    setIfUnfocused(h_editor_.get(), sel->height());

    suppress_toggle_callback_ = true;
    visible_toggle_->setToggled(sel->isVisible());
    visible_toggle_->setText(sel->isVisible() ? "on" : "off");
    on_top_toggle_->setToggled(sel->isOnTop());
    on_top_toggle_->setText(sel->isOnTop() ? "on" : "off");
    ignores_mouse_toggle_->setToggled(sel->ignoresMouseEvents());
    ignores_mouse_toggle_->setText(sel->ignoresMouseEvents() ? "on" : "off");
    suppress_toggle_callback_ = false;
}

void InspectorPropertiesView::commitBoundsFromEditors() {
    auto* sel = window_.selectedFrame();
    if (!sel) return;
    auto parse = [](applause::TextEditor* e, float fallback) {
        const std::string s = e->text().toUtf8();
        if (s.empty()) return fallback;
        char* end = nullptr;
        float v = std::strtof(s.c_str(), &end);
        return (end == s.c_str()) ? fallback : v;
    };
    const float x = parse(x_editor_.get(), sel->x());
    const float y = parse(y_editor_.get(), sel->y());
    const float w = parse(w_editor_.get(), sel->width());
    const float h = parse(h_editor_.get(), sel->height());
    sel->setBounds(x, y, w, h);
    refreshEditorsFromFrame();
}

void InspectorPropertiesView::drawRow(applause::Canvas& canvas, int row,
                                      const char* key, const std::string& value) {
    float y = static_cast<float>(row) * kRowHeight;
    canvas.setColor(ApplauseInspectorPropertiesKey);
    canvas.text(applause::String(key), label_font_, applause::Font::kLeft,
                kPaddingX, y, kKeyColumn, kRowHeight);
    canvas.setColor(ApplauseInspectorPropertiesValue);
    float vx = kPaddingX + kKeyColumn;
    canvas.text(applause::String(value), label_font_, applause::Font::kLeft,
                vx, y, width() - vx - kPaddingX, kRowHeight);
}

void InspectorPropertiesView::draw(applause::Canvas& canvas) {
    auto* sel = window_.selectedFrame();
    if (!sel) {
        canvas.setColor(ApplauseInspectorPropertiesKey);
        canvas.text(applause::String("(no selection — click a row or use Pick mode)"),
                    label_font_, applause::Font::kCenter,
                    0, 0, width(), height());
        return;
    }

    drawRow(canvas, kRowType, "type", frameTypeName(*sel));
    drawRow(canvas, kRowName, "name",
            sel->name().empty() ? std::string("—") : sel->name());

    // Bounds / visible / on-top / ignores-mouse: only the key label here;
    // the value area is covered by the live child editors / toggles.
    canvas.setColor(ApplauseInspectorPropertiesKey);
    auto drawKeyOnly = [&](int row, const char* key) {
        canvas.text(applause::String(key), label_font_, applause::Font::kLeft,
                    kPaddingX, static_cast<float>(row) * kRowHeight,
                    kKeyColumn, kRowHeight);
    };
    drawKeyOnly(kRowBounds,       "bounds");
    drawKeyOnly(kRowVisible,      "visible");
    drawKeyOnly(kRowOnTop,        "on top");
    drawKeyOnly(kRowIgnoresMouse, "ignores mouse");

    if (auto* p = sel->parent())
        drawRow(canvas, kRowParent, "parent", frameTypeName(*p));
    else
        drawRow(canvas, kRowParent, "parent", "—");
    drawRow(canvas, kRowChildren, "children",
            std::to_string(sel->children().size()));
    drawRow(canvas, kRowSubtreeSize, "subtree size",
            std::to_string(recursiveFrameCount(sel)));
}

void InspectorPropertiesView::resized() {
    const float vx = kPaddingX + kKeyColumn;
    const float value_w = width() - vx - kPaddingX;

    // Bounds row: four editors evenly spaced across the value column.
    const float bounds_y = static_cast<float>(kRowBounds) * kRowHeight + 2.0f;
    const float widget_h = kRowHeight - 4.0f;
    constexpr float kCellGap = 4.0f;
    const float cell_w = (value_w - 3.0f * kCellGap) / 4.0f;
    applause::TextEditor* editors[] = {x_editor_.get(), y_editor_.get(),
                                       w_editor_.get(), h_editor_.get()};
    for (int i = 0; i < 4; ++i) {
        editors[i]->setBounds(vx + i * (cell_w + kCellGap), bounds_y,
                              cell_w, widget_h);
    }

    // Toggle rows: single small pill at the start of the value column.
    constexpr float kToggleW = 44.0f;
    auto placeToggle = [&](applause::ToggleTextButton* t, int row) {
        t->setBounds(vx, static_cast<float>(row) * kRowHeight + 2.0f,
                     kToggleW, widget_h);
    };
    placeToggle(visible_toggle_.get(),       kRowVisible);
    placeToggle(on_top_toggle_.get(),        kRowOnTop);
    placeToggle(ignores_mouse_toggle_.get(), kRowIgnoresMouse);
}

}  // namespace applause::inspector

#endif  // NDEBUG
