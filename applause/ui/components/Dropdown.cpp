#include "Dropdown.h"

#include <algorithm>

namespace applause {
namespace {
const PopupMenu* findOption(const PopupMenu& menu, int id) {
    for (const auto& option : menu.options()) {
        if (option.isBreak()) continue;
        if (!option.hasOptions() && option.id() == id) return &option;
        if (const auto* found = findOption(option, id)) return found;
    }
    return nullptr;
}

// Copies the options of `source` into `target`, checking the selected one.
void copyOptions(const PopupMenu& source, PopupMenu& target, std::optional<int> selected) {
    for (const auto& option : source.options()) {
        if (option.isBreak()) {
            target.addBreak();
        } else if (option.hasOptions()) {
            PopupMenu sub_menu(option.name(), option.id());
            copyOptions(option, sub_menu.enable(option.enabled()), selected);
            target.addSubMenu(std::move(sub_menu));
        } else {
            target.addOption(option.id(), option.name()).enable(option.enabled()).select(option.id() == selected);
        }
    }
}
}  // namespace

Dropdown::Dropdown(const std::vector<std::string>& options) : PopupMenuButton("") { setOptions(options); }

void Dropdown::setMenu(PopupMenu menu) {
    menu_ = std::move(menu);
    changeSelection(selected_, false);
}

void Dropdown::setOptions(const std::vector<std::string>& options) {
    PopupMenu menu;
    for (int i = 0; i < static_cast<int>(options.size()); ++i) menu.addOption(i, options[i]);
    setMenu(std::move(menu));
}

void Dropdown::setPlaceholder(std::string placeholder) {
    placeholder_ = std::move(placeholder);
    changeSelection(selected_, false);
}

void Dropdown::changeSelection(std::optional<int> id, bool notify) {
    const PopupMenu* option = id ? findOption(menu_, *id) : nullptr;
    if (!option) id.reset();
    setText(option ? option->name().toUtf8() : placeholder_);
    if (id == selected_) return;

    selected_ = id;
    selectionChanged(notify);
    if (notify && selected_) on_selection_changed_.callback(*selected_);
}

void Dropdown::buildMenu(PopupMenu& menu) {
    if (onBuildMenu()) {
        PopupMenu options;
        onBuildMenu()(options);
        setMenu(std::move(options));
    }
    copyOptions(menu_, menu, selected_);
}

void Dropdown::drawContent(applause::Canvas& canvas, float hover) {
    const applause::Color black(0xff000000);
    const float padding = value(*this, kPadding);
    const float chevron = std::clamp(value(*this, kChevronWidth), 0.0f, std::max(0.0f, width() - 2.0f * padding));
    const float text_width = std::max(0.0f, width() - 3.0f * padding - chevron);

    applause::Color content = isOpen() || isPressed()
                                  ? color(*this, kTextPressed)
                                  : color(*this, kText).interpolateWith(color(*this, kTextHover), hover);
    applause::Color arrow =
        isOpen() ? color(*this, kOpen) : color(*this, kChevron).interpolateWith(color(*this, kChevronHover), hover);
    if (!isEnabled()) {
        content = content.interpolateWith(black, 0.5f);
        arrow = arrow.interpolateWith(black, 0.5f);
    }

    if (!text().isEmpty() && text_width > 0.0f) {
        canvas.setColor(content);
        canvas.text(text(), font(), applause::Font::kLeft, padding, 0.0f, text_width, height());
    }

    if (chevron <= 0.0f) return;

    // A V half as tall as it is wide, pointing up while the menu is open.
    const float x = width() - padding - chevron;
    const float center = height() * 0.5f;
    const float tip = isOpen() ? -0.25f * chevron : 0.25f * chevron;
    const float thickness = 0.18f * chevron;
    canvas.setColor(arrow);
    canvas.segment(x, center - tip, x + 0.5f * chevron, center + tip, thickness, true);
    canvas.segment(x + 0.5f * chevron, center + tip, x + chevron, center - tip, thickness, true);
}

}  // namespace applause
