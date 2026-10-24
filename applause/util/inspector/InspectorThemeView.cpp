#include "InspectorThemeView.h"

#ifndef NDEBUG

#include "FrameUtil.h"
#include "InspectorWindow.h"

#include <embedded/applause_fonts.h>
#include <applause/util/inspector/ColorPicker.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>

namespace applause::inspector {

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

// Tokens are named "group.token"; a name without a dot belongs to the "theme" group.
std::string groupOf(const std::string& name) {
    const auto dot = name.find('.');
    return dot == std::string::npos ? "theme" : name.substr(0, dot);
}

std::string labelOf(const std::string& name) {
    const auto dot = name.find('.');
    return dot == std::string::npos ? name : name.substr(dot + 1);
}

std::set<std::string> tokenGroups() {
    std::set<std::string> groups;
    for (const ColorToken* token : ColorToken::all()) groups.insert(groupOf(token->name()));
    for (const ValueToken* token : ValueToken::all()) groups.insert(groupOf(token->name()));
    return groups;
}

// What a token's default follows, like "follows accent"; empty for a fixed default. A token in the same group is named
// without its group.
template <typename T>
std::string followHint(const Token<T>& token) {
    if (!token.base()) return {};
    const std::string name = token.name();
    const std::string base = token.base()->name();
    return "follows " + (groupOf(base) == groupOf(name) ? labelOf(base) : base);
}

constexpr float kStatusGap = 8.0f;
constexpr float kResetWidth = 32.0f;

// Draws, ending at `right`, "reset" when the target sets the token, and otherwise what its default follows. Returns
// where it starts, so the label can stop short of it.
template <typename T>
float drawStatus(applause::Canvas& canvas, InspectorThemeView& view, const Token<T>& token, float right, float height) {
    const bool set_here = view.target().find(token);
    const std::string text = set_here ? "reset" : followHint(token);
    if (text.empty()) return right;

    const float width = set_here ? kResetWidth : right * 0.5f;
    canvas.setColor(color(view, set_here ? InspectorThemeView::kText : InspectorThemeView::kTextMuted));
    canvas.text(applause::String(text), view.labelFont(), applause::Font::kRight, right - width, 0, width, height);
    return right - width - kStatusGap;
}

// Whether a click at `x` hits the "reset" drawn by drawStatus.
template <typename T>
bool hitsReset(InspectorThemeView& view, const Token<T>& token, float x, float right) {
    return view.target().find(token) && x >= right - kResetWidth && x <= right;
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
    ThemeColorRow(const ColorToken& token, InspectorThemeView& view) : token_(token), view_(view) {}

    std::string stableKey() const override { return std::string("color:") + token_.name(); }

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
            canvas.setColor(color(view_, InspectorThemeView::kRowSelected));
            canvas.rectangle(0, 0, bounds.width(), bounds.height());
        }

        const float swatch_x = InspectorThemeView::kIndent + InspectorThemeView::kPaddingX;
        const float swatch_y = (bounds.height() - InspectorThemeView::kSwatchSize) * 0.5f;
        canvas.setColor(color(view_.target(), token_));
        canvas.roundedRectangle(swatch_x, swatch_y,
                                InspectorThemeView::kSwatchSize,
                                InspectorThemeView::kSwatchSize, 2.0f);
        canvas.setColor(color(view_, InspectorThemeView::kSwatchBorder));
        canvas.roundedRectangleBorder(swatch_x, swatch_y,
                                      InspectorThemeView::kSwatchSize,
                                      InspectorThemeView::kSwatchSize, 2.0f, 1.0f);

        const float status_x = drawStatus(canvas, view_, token_, statusRight(bounds.width()), bounds.height());
        canvas.setColor(color(view_, InspectorThemeView::kText));
        const float text_x = swatch_x + InspectorThemeView::kSwatchSize + 6.0f;
        canvas.text(applause::String(labelOf(token_.name())), view_.labelFont(), applause::Font::kLeft, text_x, 0,
                    std::max(0.0f, status_x - text_x), bounds.height());
    }

    void drawInlineDetail(applause::Canvas& canvas,
                          applause::Bounds bounds,
                          const RowContext&) override {
        canvas.setColor(color(view_, InspectorThemeView::kPickerBackground));
        canvas.rectangle(0, bounds.y(), bounds.width(), bounds.height());
    }

    void onHeaderClick(applause::Point local, const RowContext&) override {
        if (hitsReset(view_, token_, local.x, statusRight(width_)))
            view_.reset(token_);
        else if (isEditing())
            view_.clearEditingColor();
        else
            view_.setEditingColor(token_);
    }

    void layoutContent(applause::Frame& row_frame, applause::Bounds header, applause::Bounds detail,
                       bool expanded) override {
        width_ = header.width();
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
    bool isEditing() const { return view_.editingColor() == &token_; }
    static float statusRight(float width) { return width - InspectorThemeView::kPaddingX; }

    const ColorToken& token_;
    InspectorThemeView& view_;
    float width_ = 0.0f;  // The header's, from the last layout.
};

// ---- ThemeValueRow -------------------------------------------------------

class ThemeValueRow : public TreeRow {
public:
    ThemeValueRow(const ValueToken& token, InspectorThemeView& view) :
        token_(token), view_(view), editor_(std::make_unique<applause::TextEditor>("inspector_value_editor")) {
        editor_->setNumberEntry();
        editor_->setMaxCharacters(10);
        editor_->setFont(applause::Font(InspectorThemeView::kLabelFontSize,
                                        applause::fonts::Barlow_Medium_ttf));
        editor_->onEnterKey() += [this]() { view_.applyValue(token_, editor_->text().toUtf8()); };
        editor_->onEscapeKey() += [this]() {
            // Rebuild so layoutContent refreshes our text from the editor.
            if (auto* t = tree()) t->rebuild();
        };
    }

    ~ThemeValueRow() override = default;

    std::string stableKey() const override { return std::string("value:") + token_.name(); }

    float headerHeight() const override { return InspectorThemeView::kRowHeight; }

    void drawHeader(applause::Canvas& canvas,
                    applause::Bounds bounds,
                    const RowContext&) override {
        const float status_x = drawStatus(canvas, view_, token_, statusRight(bounds.width()), bounds.height());
        canvas.setColor(color(view_, InspectorThemeView::kText));
        const float text_x = InspectorThemeView::kIndent + InspectorThemeView::kPaddingX;
        canvas.text(applause::String(labelOf(token_.name())), view_.labelFont(), applause::Font::kLeft, text_x, 0,
                    std::max(0.0f, status_x - text_x), bounds.height());
    }

    void onHeaderClick(applause::Point local, const RowContext&) override {
        if (hitsReset(view_, token_, local.x, statusRight(width_))) view_.reset(token_);
    }

    void layoutContent(applause::Frame& row_frame,
                       applause::Bounds header,
                       applause::Bounds /*detail*/,
                       bool /*expanded*/) override {
        width_ = header.width();
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

        // Re-pull the current value from the target when the user isn't
        // mid-edit; otherwise the in-progress text gets clobbered.
        if (!editor_->hasKeyboardFocus()) editor_->setText(formatValue(value(view_.target(), token_)));
    }

private:
    // The status sits just left of the value field.
    static float statusRight(float width) {
        return width - InspectorThemeView::kValueEditorW - 2.0f * InspectorThemeView::kPaddingX;
    }

    const ValueToken& token_;
    InspectorThemeView& view_;
    std::unique_ptr<applause::TextEditor> editor_;
    float width_ = 0.0f;  // The header's, from the last layout.
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
        for (const ColorToken* token : ColorToken::all()) {
            if (groupOf(token->name()) == group_) out.push_back(std::make_unique<ThemeColorRow>(*token, view_));
        }
        for (const ValueToken* token : ValueToken::all()) {
            if (groupOf(token->name()) == group_) out.push_back(std::make_unique<ThemeValueRow>(*token, view_));
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

        // Inactive groups paint no background — let the panel show through,
        // matching the component tree's flat look. The active variant remains
        // a deliberate visual cue.
        if (active) {
            canvas.setColor(color(view_, InspectorThemeView::kGroupBackgroundActive));
            canvas.rectangle(0, 0, bounds.width(), bounds.height());
        }

        const float tri_x = InspectorThemeView::kPaddingX;
        const float tri_cy = bounds.height() * 0.5f;
        const float t = InspectorThemeView::kTriangleSize;
        canvas.setColor(color(view_, InspectorThemeView::kTextMuted));
        if (ctx.expanded) {
            canvas.triangle(tri_x,            tri_cy - t * 0.3f,
                            tri_x + t,        tri_cy - t * 0.3f,
                            tri_x + t * 0.5f, tri_cy + t * 0.5f);
        } else {
            canvas.triangle(tri_x,            tri_cy - t * 0.5f,
                            tri_x + t * 0.8f, tri_cy,
                            tri_x,            tri_cy + t * 0.5f);
        }

        canvas.setColor(color(view_, InspectorThemeView::kText));
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

// Synthetic root that enumerates every token group. Its header names the
// frame edits go to; the tree always traverses into it since "theme:root"
// is kept in expanded_keys_, and clicking it doesn't collapse it.
class ThemeRootRow : public TreeRow {
public:
    explicit ThemeRootRow(InspectorThemeView& view) : view_(view) {}

    std::string stableKey() const override { return "theme:root"; }
    bool isBranch() const override { return true; }
    float headerHeight() const override { return InspectorThemeView::kHeaderHeight; }
    void onHeaderClick(applause::Point, const RowContext&) override {}

    void drawHeader(applause::Canvas& canvas, applause::Bounds bounds, const RowContext&) override {
        const applause::Frame& target = view_.target();
        std::string text = "Editing " + frameTypeName(target);
        if (!target.name().empty()) text += " \"" + target.name() + "\"";
        canvas.setColor(color(view_, InspectorThemeView::kTextMuted));
        canvas.text(applause::String(text), view_.labelFont(), applause::Font::kLeft, InspectorThemeView::kPaddingX, 0,
                    bounds.width() - 2.0f * InspectorThemeView::kPaddingX, bounds.height());
    }

    std::vector<std::unique_ptr<TreeRow>> buildChildren() override {
        // std::set gives us alphabetical order.
        const std::set<std::string> groups = tokenGroups();
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
      label_font_(kLabelFontSize, applause::fonts::Barlow_Medium_ttf),
      color_picker_(std::make_unique<applause::ColorPicker>()) {
    setName("InspectorThemeView");

    color_picker_->setVisible(false);
    color_picker_->onColorChange() += [this](const applause::Color& c) {
        if (suppress_picker_callback_) return;
        applyEditedColor(c);
    };

    setRoot(std::make_unique<ThemeRootRow>(*this));
    expandKey("theme:root");

    // Phase 2: react to selection. Additively auto-expand groups whose name
    // overlaps the selected frame's dynamic class name; the user's manual
    // expansions stay open across selections.
    window_.onSelectionChanged() += [this](applause::Frame* f) {
        // The selection is the new target, so an open picker would edit the wrong frame.
        editing_color_ = nullptr;
        applySelectionExpansion(f);
        rebuild();
    };
}

InspectorThemeView::~InspectorThemeView() = default;

void InspectorThemeView::setEditingColor(const ColorToken& token) {
    editing_color_ = &token;
    seedPicker();
    rebuild();
}

void InspectorThemeView::seedPicker() {
    // Without re-firing onColorChange, which would set the same value on the
    // target and trigger spurious redraws.
    suppress_picker_callback_ = true;
    color_picker_->setColor(color(target(), *editing_color_));
    suppress_picker_callback_ = false;
}

applause::Frame& InspectorThemeView::target() const {
    return window_.selectedFrame() ? *window_.selectedFrame() : window_.editor();
}

void InspectorThemeView::reset(const ColorToken& token) {
    target().unset(token);
    if (editing_color_ == &token) seedPicker();
    rebuild();
}

void InspectorThemeView::reset(const ValueToken& token) {
    target().unset(token);
    rebuild();
}

void InspectorThemeView::clearEditingColor() {
    editing_color_ = nullptr;
    // Drop any keyboard focus held by the picker's text editors before they
    // become invisible — otherwise typing would go to a hidden field.
    requestKeyboardFocus();
    rebuild();
}

void InspectorThemeView::applyEditedColor(const applause::Color& color) {
    if (!editing_color_) return;
    target().set(*editing_color_, color);
    // Every row, so the edited one shows its reset.
    redrawAll();
}

void InspectorThemeView::applyValue(const ValueToken& token, const std::string& text) {
    auto trimmed = text;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front())))
        trimmed.erase(0, 1);
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back())))
        trimmed.pop_back();
    if (trimmed.empty()) {
        target().unset(token);
    } else {
        char* end = nullptr;
        float v = std::strtof(trimmed.c_str(), &end);
        if (end != trimmed.c_str()) target().set(token, v);
    }
    rebuild();
}

std::vector<std::string> InspectorThemeView::matchingGroupsFor(applause::Frame* frame) {
    if (!frame) return {};
    std::vector<std::string> candidates;
    candidates.push_back(normalize(demangle(typeid(*frame).name())));
    if (!frame->name().empty())
        candidates.push_back(normalize(frame->name()));

    std::vector<std::string> matches;
    for (const auto& g : tokenGroups()) {
        std::string g_norm = normalize(g);
        for (const auto& cand : candidates) {
            if (containsCI(cand, g_norm)) {
                matches.push_back(g);
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
