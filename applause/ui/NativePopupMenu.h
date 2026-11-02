#pragma once

#include <applause/ui/ApplauseUI.h>

#include <string>
#include <vector>

namespace applause {

/**
 * @class NativePopupMenu
 * @brief A native platform popup/context menu implementation.
 *
 * Provides a fluent API for building and displaying native context menus: NSMenu on macOS and Win32 menus on
 * Windows. Linux falls back to the drawn PopupMenu.
 */
class NativePopupMenu {
public:
    enum class Modifier : int {
        None = 0,
        Cmd = 1 << 0,
        Option = 1 << 1,
        Ctrl = 1 << 2,
        Shift = 1 << 3
    };

    friend constexpr Modifier operator|(Modifier a, Modifier b) {
        return static_cast<Modifier>(static_cast<int>(a) | static_cast<int>(b));
    }

    friend constexpr Modifier operator&(Modifier a, Modifier b) {
        return static_cast<Modifier>(static_cast<int>(a) & static_cast<int>(b));
    }

    NativePopupMenu();

    explicit NativePopupMenu(const std::string &name, int id = -1);

    ~NativePopupMenu();

    NativePopupMenu(NativePopupMenu &&other) noexcept;

    NativePopupMenu &operator=(NativePopupMenu &&other) noexcept;

    NativePopupMenu(const NativePopupMenu &) = delete;

    NativePopupMenu &operator=(const NativePopupMenu &) = delete;

    /**
     * @brief Add a selectable menu option.
     * @param id Unique identifier for this option (returned in callbacks)
     * @param name Display text for the option
     * @return Reference to the newly created option for chaining
     */
    NativePopupMenu &addOption(int id, const std::string &name);

    /**
     * @brief Add a separator/divider line.
     */
    void addBreak();

    /**
     * @brief Add a submenu.
     * @param name Display text for the submenu
     * @return Reference to the submenu for adding child options
     */
    NativePopupMenu &addSubMenu(const std::string &name);

    /**
     * @brief Enable or disable this menu item.
     * @param enabled Whether the item should be enabled
     * @return Reference to this for chaining
     */
    NativePopupMenu &enable(bool enabled);

    /**
     * @brief Set the selected/checked state of this menu item.
     * @param selected Whether the item should show a checkmark
     * @return Reference to this for chaining
     */
    NativePopupMenu &select(bool selected);

    /**
     * @brief Attach a keyboard shortcut to this menu item.
     * @param modifiers Combination of Modifier flags (e.g., Modifier::Cmd | Modifier::Shift)
     * @param key The key character (e.g., "k", "s")
     * @return Reference to this for chaining
     */
    NativePopupMenu &withKeyboardShortcut(Modifier modifiers, const std::string &key);

    /// Callback triggered when this specific option is selected
    auto &onSelection() { return on_selection_; }
    const auto &onSelection() const { return on_selection_; }

    /// Callback triggered when the menu is cancelled (dismissed without selection)
    auto &onCancel() { return on_cancel_; }
    const auto &onCancel() const { return on_cancel_; }

    /**
     * @brief Display the popup menu at the specified position.
     *
     * On macOS and Windows this blocks until the menu closes. The drawn Linux menu returns immediately and runs the
     * callbacks later.
     *
     * @param source Frame the menu opens from; it must be inside an ApplicationEditor's window
     * @param position Top-left corner of the menu, relative to source
     */
    void show(Frame *source, Point position);

    /**
     * @brief Display the menu as the drawn, themed PopupMenu instead, on any platform.
     *
     * Returns immediately and runs the callbacks later.
     *
     * @param source Frame the menu opens from
     * @param position Top-left corner of the menu, relative to source, or PopupMenu::kNotSet to open below source
     */
    void showDrawn(Frame *source, Point position);

    [[nodiscard]] const std::string &name() const { return name_; }
    [[nodiscard]] int id() const { return id_; }
    [[nodiscard]] bool isBreak() const { return is_break_; }
    [[nodiscard]] bool isEnabled() const { return enabled_; }
    [[nodiscard]] bool isSelected() const { return selected_; }
    [[nodiscard]] Modifier shortcutModifiers() const { return shortcut_modifiers_; }
    [[nodiscard]] const std::string &shortcutKey() const { return shortcut_key_; }
    [[nodiscard]] const std::vector<NativePopupMenu> &options() const { return options_; }
    [[nodiscard]] bool hasOptions() const { return !options_.empty(); }

private:
    std::string name_;
    int id_ = -1;
    bool is_break_ = false;
    bool enabled_ = true;
    bool selected_ = false;
    Modifier shortcut_modifiers_ = Modifier::None;
    std::string shortcut_key_;
    std::vector<NativePopupMenu> options_;
    applause::CallbackList<void(int)> on_selection_;
    applause::CallbackList<void()> on_cancel_;
};
} // namespace applause
