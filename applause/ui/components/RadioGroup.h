#pragma once

#include <applause/ui/ApplauseUI.h>

#include <functional>
#include <vector>

namespace applause {

class ToggleButton;
class ParamRadioGroup;

/// A non-owning group of buttons with one selected member whenever the group is nonempty.
/// Membership is explicit and does not depend on the buttons' frame parents.
class RadioGroup {
public:
    RadioGroup() = default;
    ~RadioGroup();
    RadioGroup(const RadioGroup&) = delete;
    RadioGroup& operator=(const RadioGroup&) = delete;
    RadioGroup(RadioGroup&&) = delete;
    RadioGroup& operator=(RadioGroup&&) = delete;

    /// IDs must be unique and nonnegative. The first member is selected silently.
    void addButton(ToggleButton& button, int id);
    /// Removes a member silently, selecting the first survivor if necessary.
    /// Removing a button that is not a member does nothing.
    /// Explicit membership changes are forbidden while a parameter binding is attached.
    void removeButton(ToggleButton& button);
    /// Replaces a member at the same ID, preserving selection without notifications.
    void replaceButton(int id, ToggleButton& replacement);

    /// Returns -1 only when the group is empty.
    [[nodiscard]] int selectedId() const noexcept { return selected_id_; }
    void setSelectedId(int id);
    void setSelectedIdAndNotify(int id);
    auto& onSelectionChanged() { return on_selection_changed_; }

private:
    friend class ToggleButton;
    friend class ParamRadioGroup;

    struct Member {
        ToggleButton* button;
        int id;
    };

    ToggleButton* findButton(int id) const;
    int findId(const ToggleButton& button) const;
    void validateNewButton(const ToggleButton& button) const;
    void select(int id, bool notify);
    void setButtonState(ToggleButton& button, bool selected, bool notify);
    void buttonDestroyed(ToggleButton& button);
    void removeMember(ToggleButton& button);
    void invalidateBinding();

    std::vector<Member> members_;
    int selected_id_ = -1;
    applause::CallbackList<void(int)> on_selection_changed_;
    std::function<void(int)> parameter_changed_;
    std::function<void()> binding_invalidated_;
};

}  // namespace applause
