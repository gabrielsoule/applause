#include "RadioGroup.h"

#include <applause/ui/components/Button.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace applause {

RadioGroup::~RadioGroup() {
    invalidateBinding();
    for (const auto& member : members_) member.button->radio_group_ = nullptr;
}

Button* RadioGroup::findButton(int id) const {
    const auto found = std::find_if(members_.begin(), members_.end(),
                                    [id](const Member& member) { return member.id == id; });
    return found == members_.end() ? nullptr : found->button;
}

int RadioGroup::findId(const Button& button) const {
    const auto found = std::find_if(members_.begin(), members_.end(),
                                    [&button](const Member& member) { return member.button == &button; });
    return found == members_.end() ? -1 : found->id;
}

void RadioGroup::validateNewButton(const Button& button) const {
    if (button.radio_group_) throw std::invalid_argument("Button already belongs to a radio group");
    if (!button.allowsRadioGroup()) throw std::invalid_argument("Button does not support radio grouping");
}

void RadioGroup::join(Button& button, bool selected) {
    button.radio_group_ = this;
    button.checkable_ = true;
    if (button.checked_ != selected) {
        button.checked_ = selected;
        button.dispatchCheckedChanged(false);
    }
}

void RadioGroup::addButton(Button& button, int id) {
    if (binding_invalidated_) throw std::logic_error("Cannot change membership of a parameter-bound radio group");
    if (id < 0 || findButton(id)) throw std::invalid_argument("Radio group IDs must be unique and nonnegative");
    validateNewButton(button);

    members_.push_back({&button, id});
    if (selected_id_ < 0) selected_id_ = id;
    join(button, selected_id_ == id);
}

void RadioGroup::removeButton(Button& button) {
    if (button.radio_group_ != this) return;
    if (binding_invalidated_) throw std::logic_error("Cannot change membership of a parameter-bound radio group");
    removeMember(button);
}

void RadioGroup::removeMember(Button& button) {
    const int id = findId(button);
    if (id < 0) return;
    std::erase_if(members_, [&button](const Member& member) { return member.button == &button; });
    button.radio_group_ = nullptr;
    if (selected_id_ != id) return;

    selected_id_ = members_.empty() ? -1 : members_.front().id;
    if (!members_.empty()) {
        auto& next = *members_.front().button;
        next.checked_ = true;
        next.dispatchCheckedChanged(false);
    }
}

void RadioGroup::replaceButton(int id, Button& replacement) {
    auto* previous = findButton(id);
    if (!previous) throw std::out_of_range("Radio group ID is out of range");
    if (previous == &replacement) return;
    validateNewButton(replacement);

    const auto member = std::find_if(members_.begin(), members_.end(),
                                     [id](const Member& entry) { return entry.id == id; });
    previous->radio_group_ = nullptr;
    member->button = &replacement;
    join(replacement, selected_id_ == id);
}

void RadioGroup::setSelectedId(int id) { select(id, false); }

void RadioGroup::setSelectedIdAndNotify(int id) { select(id, true); }

void RadioGroup::select(int id, bool notify) {
    auto* next = findButton(id);
    if (!next) throw std::out_of_range("Radio group ID is out of range");
    if (selected_id_ == id) return;

    auto* previous = findButton(selected_id_);
    previous->checked_ = false;
    next->checked_ = true;
    selected_id_ = id;

    // Subclasses and the parameter binding hear about the change before any callback runs.
    previous->dispatchCheckedChanged(notify);
    if (selected_id_ != id) return;
    next->dispatchCheckedChanged(notify);
    if (!notify || selected_id_ != id) return;

    if (parameter_changed_) parameter_changed_(id);
    if (selected_id_ == id) previous->on_toggle_.callback(false);
    if (selected_id_ == id) next->on_toggle_.callback(true);
    if (selected_id_ == id) on_selection_changed_.callback(id);
}

void RadioGroup::setButtonState(Button& button, bool selected, bool notify) {
    if (selected) select(findId(button), notify);
}

void RadioGroup::invalidateBinding() {
    auto invalidated = std::move(binding_invalidated_);
    binding_invalidated_ = {};
    parameter_changed_ = {};
    if (invalidated) invalidated();
}

void RadioGroup::buttonDestroyed(Button& button) {
    invalidateBinding();
    removeMember(button);
}

}  // namespace applause
