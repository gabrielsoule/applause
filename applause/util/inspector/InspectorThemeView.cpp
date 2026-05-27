#include "InspectorThemeView.h"

#ifndef NDEBUG

#include "FrameUtil.h"
#include "InspectorWindow.h"

#include <embedded/applause_fonts.h>
#include <visage_widgets/color_picker.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <unordered_set>

namespace applause::inspector {

APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorThemeView, ApplauseInspectorThemeText,                  0xffd8d8dc);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorThemeView, ApplauseInspectorThemeTextMuted,             0xff7a7a80);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorThemeView, ApplauseInspectorThemeRowHover,              0x22ffffff);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorThemeView, ApplauseInspectorThemeRowSelected,           0x4466bbff);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorThemeView, ApplauseInspectorThemeGroupBackground,       0xff1c1c22);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorThemeView, ApplauseInspectorThemeGroupBackgroundActive, 0xff262630);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorThemeView, ApplauseInspectorThemeSwatchBorder,          0xff3a3a40);
APPLAUSE_THEME_IMPLEMENT_COLOR(InspectorThemeView, ApplauseInspectorThemePickerBackground,      0xff141418);

namespace {

// Strip namespace prefix + template args, lowercase. Used so the heuristic
// in matchingGroupsFor is tolerant of "applause::ParamKnob<float>" matching
// the "Knob" group, etc.
std::string normalize(std::string s) {
    auto cns = s.rfind("::");
    if (cns != std::string::npos) s.erase(0, cns + 2);
    auto lt = s.find('<');
    if (lt != std::string::npos) s.erase(lt);
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool containsCI(const std::string& haystack_norm, const std::string& needle_norm) {
    return !needle_norm.empty() && haystack_norm.find(needle_norm) != std::string::npos;
}

std::string formatValue(float v) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%g", v);
    return buf;
}

// Detach `f` from any current parent before reparenting. Visage's addChild
// does NOT remove from a previous parent — without this, the old parent's
// children_ vector still references the moved frame.
void detachFromParent(applause::Frame* f) {
    if (auto* p = f->parent()) p->removeChild(f);
}

// ---- ThemeColorRow -------------------------------------------------------

class ThemeColorRow : public TreeRow {
public:
    ThemeColorRow(visage::theme::ColorId id, InspectorThemeView& view)
        : id_(id), view_(view) {}

    std::string stableKey() const override {
        return "color:" + std::to_string(id_.id);
    }

    float headerHeight() const override { return InspectorThemeView::kRowHeight; }

    float inlineDetailHeight() const override {
        return isEditing()
            ? InspectorThemeView::kPickerHeight + 2.0f * InspectorThemeView::kPickerPaddingY
            : 0.0f;
    }

    void drawHeader(applause::Canvas& canvas,
                    applause::Bounds bounds,
                    const RowContext&) override {
        const bool editing = isEditing();
        if (editing) {
            canvas.setColor(InspectorThemeView::ApplauseInspectorThemeRowSelected);
            canvas.rectangle(0, 0, bounds.width(), bounds.height());
        }

        visage::Brush brush;
        if (!view_.window().palette().color({}, id_, brush))
            brush = visage::Brush::solid(visage::theme::ColorId::defaultColor(id_));

        const float swatch_x = InspectorThemeView::kIndent + InspectorThemeView::kPaddingX;
        const float swatch_y = (bounds.height() - InspectorThemeView::kSwatchSize) * 0.5f;
        canvas.setColor(brush);
        canvas.roundedRectangle(swatch_x, swatch_y,
                                InspectorThemeView::kSwatchSize,
                                InspectorThemeView::kSwatchSize, 2.0f);
        canvas.setColor(InspectorThemeView::ApplauseInspectorThemeSwatchBorder);
        canvas.roundedRectangleBorder(swatch_x, swatch_y,
                                      InspectorThemeView::kSwatchSize,
                                      InspectorThemeView::kSwatchSize, 2.0f, 1.0f);

        canvas.setColor(InspectorThemeView::ApplauseInspectorThemeText);
        const float text_x = swatch_x + InspectorThemeView::kSwatchSize + 6.0f;
        canvas.text(applause::String(visage::theme::ColorId::name(id_)),
                    view_.labelFont(), applause::Font::kLeft,
                    text_x, 0,
                    bounds.width() - text_x - InspectorThemeView::kPaddingX,
                    bounds.height());
    }

    void drawInlineDetail(applause::Canvas& canvas,
                          applause::Bounds bounds,
                          const RowContext&) override {
        canvas.setColor(InspectorThemeView::ApplauseInspectorThemePickerBackground);
        canvas.rectangle(0, bounds.y(), bounds.width(), bounds.height());
    }

    void onHeaderClick(applause::Point, const RowContext&) override {
        if (isEditing()) view_.clearEditingColor();
        else             view_.setEditingColor(id_);
    }

    void layoutContent(applause::Frame& row_frame,
                       applause::Bounds /*header*/,
                       applause::Bounds detail,
                       bool expanded) override {
        auto* picker = view_.colorPicker();
        if (!picker) return;
        if (expanded) {
            if (picker->parent() != &row_frame) {
                detachFromParent(picker);
                row_frame.addChild(picker);
            }
            const float x = InspectorThemeView::kPaddingX;
            const float y = detail.y() + InspectorThemeView::kPickerPaddingY;
            const float w = std::max(0.0f, row_frame.width() - 2.0f * InspectorThemeView::kPaddingX);
            picker->setBounds(x, y, w, InspectorThemeView::kPickerHeight);
            picker->setVisible(true);
        } else {
            if (picker->parent() == &row_frame) row_frame.removeChild(picker);
        }
    }

private:
    bool isEditing() const {
        const auto e = view_.editingColor();
        return e.isValid() && e.id == id_.id;
    }

    visage::theme::ColorId id_;
    InspectorThemeView& view_;
};

// ---- ThemeValueRow -------------------------------------------------------

class ThemeValueRow : public TreeRow {
public:
    ThemeValueRow(visage::theme::ValueId id, InspectorThemeView& view)
        : id_(id), view_(view),
          editor_(std::make_unique<applause::TextEditor>("inspector_value_editor")) {
        editor_->setNumberEntry();
        editor_->setMaxCharacters(10);
        editor_->setFont(applause::Font(InspectorThemeView::kLabelFontSize,
                                        applause::fonts::Jost_Regular_ttf));
        editor_->onEnterKey() += [this]() {
            view_.applyValue(id_, editor_->text().toUtf8());
        };
        editor_->onEscapeKey() += [this]() {
            // Rebuild so layoutContent refreshes our text from the palette.
            if (auto* t = tree()) t->rebuild();
        };
    }

    ~ThemeValueRow() override = default;

    std::string stableKey() const override {
        return "value:" + std::to_string(id_.id);
    }

    float headerHeight() const override { return InspectorThemeView::kRowHeight; }

    void drawHeader(applause::Canvas& canvas,
                    applause::Bounds bounds,
                    const RowContext&) override {
        canvas.setColor(InspectorThemeView::ApplauseInspectorThemeText);
        const float text_x = InspectorThemeView::kIndent + InspectorThemeView::kPaddingX;
        const float text_w = std::max(
            0.0f,
            bounds.width() - text_x - InspectorThemeView::kValueEditorW - 2.0f * InspectorThemeView::kPaddingX);
        canvas.text(applause::String(visage::theme::ValueId::name(id_)),
                    view_.labelFont(), applause::Font::kLeft,
                    text_x, 0, text_w, bounds.height());
    }

    void layoutContent(applause::Frame& row_frame,
                       applause::Bounds header,
                       applause::Bounds /*detail*/,
                       bool /*expanded*/) override {
        if (editor_->parent() != &row_frame) {
            detachFromParent(editor_.get());
            row_frame.addChild(editor_.get());
        }
        const float editor_x = header.width()
                             - InspectorThemeView::kValueEditorW
                             - InspectorThemeView::kPaddingX;
        editor_->setBounds(editor_x, 2.0f,
                           InspectorThemeView::kValueEditorW,
                           header.height() - 4.0f);

        // Re-pull the current value from the palette when the user isn't
        // mid-edit; otherwise the in-progress text gets clobbered.
        if (!editor_->hasKeyboardFocus()) {
            float current = 0.0f;
            if (view_.window().palette().value({}, id_, current))
                editor_->setText(formatValue(current));
            else
                editor_->setText(formatValue(visage::theme::ValueId::defaultValue(id_)));
        }
    }

private:
    visage::theme::ValueId id_;
    InspectorThemeView& view_;
    std::unique_ptr<applause::TextEditor> editor_;
};

// ---- ThemeGroupRow -------------------------------------------------------

class ThemeGroupRow : public TreeRow {
public:
    ThemeGroupRow(std::string group, InspectorThemeView& view)
        : group_(std::move(group)), view_(view) {}

    std::string stableKey() const override { return "group:" + group_; }

    bool isBranch() const override { return true; }

    float headerHeight() const override { return InspectorThemeView::kHeaderHeight; }

    std::vector<std::unique_ptr<TreeRow>> buildChildren() override {
        std::vector<std::unique_ptr<TreeRow>> out;
        const int num_colors = visage::theme::ColorId::numColorIds();
        for (int i = 0; i < num_colors; ++i) {
            visage::theme::ColorId id(static_cast<unsigned int>(i));
            if (visage::theme::ColorId::groupName(id) == group_)
                out.push_back(std::make_unique<ThemeColorRow>(id, view_));
        }
        const int num_values = visage::theme::ValueId::numValueIds();
        for (int i = 0; i < num_values; ++i) {
            visage::theme::ValueId id(static_cast<unsigned int>(i));
            if (visage::theme::ValueId::groupName(id) == group_)
                out.push_back(std::make_unique<ThemeValueRow>(id, view_));
        }
        return out;
    }

    void drawHeader(applause::Canvas& canvas,
                    applause::Bounds bounds,
                    const RowContext& ctx) override {
        // Active = matches the currently-selected frame's class name.
        auto* sel = view_.window().selectedFrame();
        const auto matches = InspectorThemeView::matchingGroupsFor(sel);
        const bool active = std::find(matches.begin(), matches.end(), group_) != matches.end();

        canvas.setColor(active
            ? InspectorThemeView::ApplauseInspectorThemeGroupBackgroundActive
            : InspectorThemeView::ApplauseInspectorThemeGroupBackground);
        canvas.rectangle(0, 0, bounds.width(), bounds.height());

        const float tri_x = InspectorThemeView::kPaddingX;
        const float tri_cy = bounds.height() * 0.5f;
        const float t = InspectorThemeView::kTriangleSize;
        canvas.setColor(InspectorThemeView::ApplauseInspectorThemeTextMuted);
        if (ctx.expanded) {
            canvas.triangle(tri_x,            tri_cy - t * 0.3f,
                            tri_x + t,        tri_cy - t * 0.3f,
                            tri_x + t * 0.5f, tri_cy + t * 0.5f);
        } else {
            canvas.triangle(tri_x,            tri_cy - t * 0.5f,
                            tri_x + t * 0.8f, tri_cy,
                            tri_x,            tri_cy + t * 0.5f);
        }

        canvas.setColor(InspectorThemeView::ApplauseInspectorThemeText);
        const float text_x = tri_x + t + 6.0f;
        canvas.text(applause::String(group_),
                    view_.labelFont(), applause::Font::kLeft,
                    text_x, 0,
                    bounds.width() - text_x - InspectorThemeView::kPaddingX,
                    bounds.height());
    }

private:
    std::string group_;
    InspectorThemeView& view_;
};

// ---- ThemeRootRow --------------------------------------------------------

// Synthetic root that enumerates all registered theme groups. headerHeight
// of 0 keeps it invisible in the list; the tree always traverses into it
// since "theme:root" is kept in expanded_keys_.
class ThemeRootRow : public TreeRow {
public:
    explicit ThemeRootRow(InspectorThemeView& view) : view_(view) {}

    std::string stableKey() const override { return "theme:root"; }
    bool isBranch() const override { return true; }
    float headerHeight() const override { return 0.0f; }
    void drawHeader(applause::Canvas&, applause::Bounds, const RowContext&) override {}

    std::vector<std::unique_ptr<TreeRow>> buildChildren() override {
        // Collect every group name registered on either ColorId or ValueId.
        // std::set gives us alphabetical order across the union.
        std::set<std::string> groups;
        const int num_colors = visage::theme::ColorId::numColorIds();
        for (int i = 0; i < num_colors; ++i) {
            visage::theme::ColorId id(static_cast<unsigned int>(i));
            groups.insert(visage::theme::ColorId::groupName(id));
        }
        const int num_values = visage::theme::ValueId::numValueIds();
        for (int i = 0; i < num_values; ++i) {
            visage::theme::ValueId id(static_cast<unsigned int>(i));
            groups.insert(visage::theme::ValueId::groupName(id));
        }
        std::vector<std::unique_ptr<TreeRow>> out;
        out.reserve(groups.size());
        for (const auto& g : groups)
            out.push_back(std::make_unique<ThemeGroupRow>(g, view_));
        return out;
    }

private:
    InspectorThemeView& view_;
};

}  // namespace

// ---- InspectorThemeView --------------------------------------------------

InspectorThemeView::InspectorThemeView(InspectorWindow& window)
    : window_(window),
      label_font_(kLabelFontSize, applause::fonts::Jost_Regular_ttf),
      color_picker_(std::make_unique<visage::ColorPicker>()) {
    setName("InspectorThemeView");

    color_picker_->setVisible(false);
    color_picker_->onColorChange() += [this](const visage::Color& c) {
        if (suppress_picker_callback_) return;
        applyEditedColor(c);
    };

    setRoot(std::make_unique<ThemeRootRow>(*this));
    expandKey("theme:root");

    // Phase 2: react to selection. Additively auto-expand groups whose name
    // overlaps the selected frame's dynamic class name; the user's manual
    // expansions stay open across selections.
    window_.onSelectionChanged() += [this](applause::Frame* f) {
        applySelectionExpansion(f);
        rebuild();
    };
}

InspectorThemeView::~InspectorThemeView() = default;

void InspectorThemeView::setEditingColor(visage::theme::ColorId id) {
    editing_color_id_ = id;

    // Seed the picker with the current resolved color without re-firing
    // onColorChange (which would write the same value back to the palette
    // and trigger spurious editor redraws).
    visage::Brush b;
    visage::Color current;
    if (window_.palette().color({}, id, b))
        current = b.gradient().sample(0.0f);
    else
        current = visage::Color(visage::theme::ColorId::defaultColor(id));
    suppress_picker_callback_ = true;
    color_picker_->setColor(current);
    suppress_picker_callback_ = false;

    rebuild();
}

void InspectorThemeView::clearEditingColor() {
    editing_color_id_ = visage::theme::ColorId();
    // Drop any keyboard focus held by the picker's text editors before they
    // become invisible — otherwise typing would go to a hidden field.
    requestKeyboardFocus();
    rebuild();
}

void InspectorThemeView::applyEditedColor(const visage::Color& color) {
    if (!editing_color_id_.isValid()) return;
    window_.palette().setColor({}, editing_color_id_, color);
    window_.editor().redrawAll();
    redraw();
}

void InspectorThemeView::applyValue(visage::theme::ValueId id, const std::string& text) {
    auto trimmed = text;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front())))
        trimmed.erase(0, 1);
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())))
        trimmed.pop_back();
    if (trimmed.empty()) {
        window_.palette().removeValue({}, id);
    } else {
        char* end = nullptr;
        float v = std::strtof(trimmed.c_str(), &end);
        if (end != trimmed.c_str())
            window_.palette().setValue({}, id, v);
    }
    window_.editor().redrawAll();
    rebuild();
}

std::vector<std::string> InspectorThemeView::matchingGroupsFor(applause::Frame* frame) {
    if (!frame) return {};
    std::vector<std::string> candidates;
    candidates.push_back(normalize(demangle(typeid(*frame).name())));
    if (!frame->name().empty())
        candidates.push_back(normalize(frame->name()));

    std::vector<std::string> matches;
    std::unordered_set<std::string> seen;
    const int num_colors = visage::theme::ColorId::numColorIds();
    for (int i = 0; i < num_colors; ++i) {
        visage::theme::ColorId id(static_cast<unsigned int>(i));
        const std::string& g = visage::theme::ColorId::groupName(id);
        if (seen.count(g)) continue;
        std::string g_norm = normalize(g);
        for (const auto& cand : candidates) {
            if (containsCI(cand, g_norm)) {
                matches.push_back(g);
                seen.insert(g);
                break;
            }
        }
    }
    const int num_values = visage::theme::ValueId::numValueIds();
    for (int i = 0; i < num_values; ++i) {
        visage::theme::ValueId id(static_cast<unsigned int>(i));
        const std::string& g = visage::theme::ValueId::groupName(id);
        if (seen.count(g)) continue;
        std::string g_norm = normalize(g);
        for (const auto& cand : candidates) {
            if (containsCI(cand, g_norm)) {
                matches.push_back(g);
                seen.insert(g);
                break;
            }
        }
    }
    return matches;
}

void InspectorThemeView::applySelectionExpansion(applause::Frame* frame) {
    for (const auto& g : matchingGroupsFor(frame))
        expandKey("group:" + g);
}

}  // namespace applause::inspector

#endif  // NDEBUG
