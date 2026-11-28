#pragma once

#include <applause/ui/PopupMenu.h>
#include <applause/ui/components/PopupMenuButton.h>

#include <optional>
#include <string>
#include <vector>

namespace applause {

// A menu button that shows the selected option and checks it in the menu. It draws no label; put one beside it, as
// GenericParameterEntry does. The options are a menu, so they can have separators and submenus; the IDs of the
// options that can be picked must be unique.
class Dropdown : public PopupMenuButton {
public:
    static inline const ColorToken kChevron{"dropdown.chevron", 0xff848789};
    static inline const ColorToken kChevronHover{"dropdown.chevronHover", 0xffaaacad};

    static inline const ValueToken kChevronWidth{"dropdown.chevronWidth", 8.0f};

    explicit Dropdown(const std::vector<std::string>& options = {});

    // Keeps the selected ID while the menu still has it, and otherwise selects nothing. Doesn't run
    // onSelectionChanged(). When onBuildMenu() is set, it replaces the menu each time the menu opens.
    void setMenu(PopupMenu menu);
    // A flat menu whose IDs are the options' positions.
    void setOptions(const std::vector<std::string>& options);

    std::optional<int> selectedId() const { return selected_; }
    // An ID that isn't in the menu selects nothing. Doesn't run onSelectionChanged().
    void setSelectedId(std::optional<int> id) { changeSelection(id, false); }

    // Shown while nothing is selected.
    void setPlaceholder(std::string placeholder);

    // Runs with the new ID when a pick from the menu changes the selection.
    auto& onSelectionChanged() { return on_selection_changed_; }

protected:
    // Called after the selection changes, before onSelectionChanged() runs. `notify` is false for silent changes.
    virtual void selectionChanged(bool notify) {}

    void buildMenu(PopupMenu& menu) override;
    void itemSelected(int id) override { changeSelection(id, true); }
    void drawContent(applause::Canvas& canvas, float hover) override;

private:
    void changeSelection(std::optional<int> id, bool notify);

    PopupMenu menu_;
    std::string placeholder_;
    std::optional<int> selected_;
    applause::CallbackList<void(int)> on_selection_changed_;
};

}  // namespace applause
