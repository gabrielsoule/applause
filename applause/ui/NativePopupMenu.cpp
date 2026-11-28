#include "PopupMenu.h"

#include <applause/ui/ApplauseUI.h>

#if defined(_WIN32)
#include <windows.h>

#include <cctype>
#include <string>
#include <vector>
#endif

namespace applause {

#if defined(_WIN32)
namespace {
std::wstring itemText(const PopupMenu& option) {
    std::string text;
    for (char c : option.name().toUtf8()) {
        // A single '&' marks a mnemonic in Win32 menus.
        if (c == '&') text += '&';
        text += c;
    }

    const std::string& key = option.nativeShortcutCharacter();
    if (!key.empty()) {
        const int modifiers = option.nativeShortcutModifiers();
        text += '\t';
        if (modifiers & (kModifierCmd | kModifierRegCtrl | kModifierMacCtrl)) text += "Ctrl+";
        if (modifiers & kModifierAlt) text += "Alt+";
        if (modifiers & kModifierShift) text += "Shift+";
        for (char c : key) text += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return String::convertToWide(text);
}

// Command ids are indices into items plus one, because TrackPopupMenuEx returns 0 when the menu is cancelled.
HMENU buildMenu(const PopupMenu& menu, std::vector<const PopupMenu*>& items) {
    HMENU handle = CreatePopupMenu();
    for (const auto& option : menu.options()) {
        if (option.isBreak()) {
            AppendMenuW(handle, MF_SEPARATOR, 0, nullptr);
            continue;
        }

        UINT flags = MF_STRING | (option.enabled() ? MF_ENABLED : MF_GRAYED) |
                     (option.selected() ? MF_CHECKED : MF_UNCHECKED);
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

void PopupMenu::showNative(Frame* source, Point position) const {
    auto* editor = dynamic_cast<ApplicationEditor*>(source);
    if (!editor) editor = source->findParent<ApplicationEditor>();
    if (!editor || !editor->window()) return;

    auto* window = editor->window();
    HWND hwnd = static_cast<HWND>(window->nativeHandle());
    IPoint client_position =
        window->convertToNative(source->positionInWindow() + position * source->scaleInWindow());
    POINT screen_position = {client_position.x, client_position.y};
    ClientToScreen(hwnd, &screen_position);

    std::vector<const PopupMenu*> items;
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
void PopupMenu::showNative(Frame* source, Point position) const { PopupMenu(*this).show(source, position); }
#endif

}  // namespace applause
