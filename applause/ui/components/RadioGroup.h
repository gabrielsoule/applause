#pragma once

#include <applause/ui/ApplauseUI.h>

#include <functional>
#include <vector>

namespace applause {

class Button;
class ParamRadioGroup;

/// A non-owning group of buttons with one selected member whenever the group is nonempty.
/// Membership is explicit and does not depend on the buttons' frame parents. Members become checkable.
class RadioGroup {
public:
    RadioGroup() = default;
    ~RadioGroup();
    RadioGroup(const RadioGroup&) = delete;
    RadioGroup& operator=(const RadioGroup&) = delete;
    RadioGroup(RadioGroup&&) = delete;
    RadioGroup& operator=(RadioGroup&&) = delete;

    /// IDs must be unique and nonnegative. The first member is selected silently.
    void addButton(Button& button, int id);
    /// Removes a member silently, selecting the first survivor if necessary.
    /// Removing a button that is not a member does nothing.
    /// Explicit membership changes are forbidden while a parameter binding is attached.
    void removeButton(Button& button);
    /// Replaces a member at the same ID, preserving selection without notifications.
    void replaceButton(int id, Button& replacement);

    /// Returns -1 only when the group is empty.
    [[nodiscard]] int selectedId() const noexcept { return selected_id_; }
    void setSelectedId(int id);
    void setSelectedIdAndNotify(int id);
    auto& onSelectionChanged() { return on_selection_changed_; }

private:
    friend class Button;
    friend class ParamRadioGroup;

    struct Member {
        Button* button;
        int id;
    };

    Button* findButton(int id) const;
    int findId(const Button& button) const;
    void validateNewButton(const Button& button) const;
    // Makes a new member checkable and sets its state silently.
    void join(Button& button, bool selected);
    void select(int id, bool notify);
    void setButtonState(Button& button, bool selected, bool notify);
    void buttonDestroyed(Button& button);
    void removeMember(Button& button);
    void invalidateBinding();

    std::vector<Member> members_;
    int selected_id_ = -1;
    applause::CallbackList<void(int)> on_selection_changed_;
    std::function<void(int)> parameter_changed_;
    std::function<void()> binding_invalidated_;
};

}  // namespace applause
