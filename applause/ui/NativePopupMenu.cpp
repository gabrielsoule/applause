#include "NativePopupMenu.h"

#include <applause/ui/PopupMenu.h>

#if defined(_WIN32)
#include <windows.h>

#include <cctype>
#endif

namespace applause {

NativePopupMenu::NativePopupMenu() = default;
NativePopupMenu::NativePopupMenu(const std::string& name, int id) : name_(name), id_(id) {}
NativePopupMenu::~NativePopupMenu() = default;
NativePopupMenu::NativePopupMenu(NativePopupMenu&& other) noexcept = default;
NativePopupMenu& NativePopupMenu::operator=(NativePopupMenu&& other) noexcept = default;

NativePopupMenu& NativePopupMenu::addOption(int option_id, const std::string& option_name) {
    options_.emplace_back(option_name, option_id);
    return options_.back();
}

void NativePopupMenu::addBreak() {
    NativePopupMenu separator;
    separator.is_break_ = true;
    options_.push_back(std::move(separator));
}

NativePopupMenu& NativePopupMenu::addSubMenu(const std::string& name) {
    options_.emplace_back(name);
    return options_.back();
}

NativePopupMenu& NativePopupMenu::enable(bool enabled) {
    enabled_ = enabled;
    return *this;
}

NativePopupMenu& NativePopupMenu::select(bool selected) {
    selected_ = selected;
    return *this;
}

NativePopupMenu& NativePopupMenu::withKeyboardShortcut(Modifier modifiers, const std::string& key) {
    shortcut_modifiers_ = modifiers;
    shortcut_key_ = key;
    return *this;
}

namespace {
PopupMenu toPopupMenu(const NativePopupMenu& menu) {
    PopupMenu result(menu.name(), menu.id(), {}, menu.isBreak());
    result.enable(menu.isEnabled()).select(menu.isSelected());
    result.onSelection() = menu.onSelection();
    result.onCancel() = menu.onCancel();
    for (const auto& option : menu.options()) result.addSubMenu(toPopupMenu(option));
    return result;
}
}  // namespace

void NativePopupMenu::showDrawn(Frame* source, Point position) { toPopupMenu(*this).show(source, position); }

#if defined(_WIN32)
namespace {
std::wstring itemText(const NativePopupMenu& option) {
    std::string text;
    for (char c : option.name()) {
        // A single '&' marks a mnemonic in Win32 menus.
        if (c == '&') text += '&';
        text += c;
    }

    if (!option.shortcutKey().empty()) {
        using Modifier = NativePopupMenu::Modifier;
        auto has = [&](Modifier modifier) { return (option.shortcutModifiers() & modifier) != Modifier::None; };
        text += '\t';
        if (has(Modifier::Cmd) || has(Modifier::Ctrl)) text += "Ctrl+";
        if (has(Modifier::Option)) text += "Alt+";
        if (has(Modifier::Shift)) text += "Shift+";
        for (char c : option.shortcutKey()) text += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return String::convertToWide(text);
}

// Command ids are indices into items plus one, because TrackPopupMenuEx returns 0 when the menu is cancelled.
HMENU buildMenu(const NativePopupMenu& menu, std::vector<const NativePopupMenu*>& items) {
    HMENU handle = CreatePopupMenu();
    for (const auto& option : menu.options()) {
        if (option.isBreak()) {
            AppendMenuW(handle, MF_SEPARATOR, 0, nullptr);
            continue;
        }

        UINT flags = MF_STRING | (option.isEnabled() ? MF_ENABLED : MF_GRAYED) |
                     (option.isSelected() ? MF_CHECKED : MF_UNCHECKED);
        UINT_PTR id = 0;
        if (option.hasOptions()) {
            flags |= MF_POPUP;
            id = reinterpret_cast<UINT_PTR>(buildMenu(option, items));
        } else {
            items.push_back(&option);
            id = items.size();
        }
        AppendMenuW(handle, flags, id, itemText(option).c_str());
    }
    return handle;
}
}  // namespace

void NativePopupMenu::show(Frame* source, Point position) {
    auto* editor = dynamic_cast<ApplicationEditor*>(source);
    if (!editor) editor = source->findParent<ApplicationEditor>();
    if (!editor || !editor->window()) return;

    auto* window = editor->window();
    HWND hwnd = static_cast<HWND>(window->nativeHandle());
    IPoint client_position = window->convertToNative(source->positionInWindow() + position);
    POINT screen_position = {client_position.x, client_position.y};
    ClientToScreen(hwnd, &screen_position);

    std::vector<const NativePopupMenu*> items;
    HMENU menu = buildMenu(*this, items);
    int command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, screen_position.x,
                                   screen_position.y, hwnd, nullptr);
    DestroyMenu(menu);

    if (command > 0) {
        const auto* selected = items[command - 1];
        selected->onSelection().callback(selected->id());
        on_selection_.callback(selected->id());
    } else {
        on_cancel_.callback();
    }
}
#elif !defined(__APPLE__)
// Linux plugin windows have no native menu toolkit to call into, so the menu is drawn instead.
void NativePopupMenu::show(Frame* source, Point position) { showDrawn(source, position); }
#endif

}  // namespace applause
