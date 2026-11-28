#include "PopupMenuButton.h"

#include <applause/ui/PopupMenu.h>

namespace applause {

PopupMenuButton::PopupMenuButton(std::string text) : Button(std::move(text)) {
    setTriggeredOnMouseDown(true);
    onClick() += [this] { showPopup(); };
}

void PopupMenuButton::setNative(bool native) {
    native_ = native;
    setTriggeredOnMouseDown(!native);
}

void PopupMenuButton::buildMenu(PopupMenu& menu) {
    if (on_build_menu_) on_build_menu_(menu);
}

void PopupMenuButton::setOpen(bool open) {
    open_ = open;
    redraw();
}

void PopupMenuButton::showPopup() {
    // A press on the button moves keyboard focus away from its open menu, which cancels the menu just before the
    // press reaches the button. That press only closes the menu.
    if (std::chrono::steady_clock::now() - cancelled_at_ < std::chrono::milliseconds(100)) return;

    PopupMenu menu;
    buildMenu(menu);
    if (!menu.hasOptions()) return;

    // The drawn menu stays open after showPopup() returns, so its callbacks check that the button still exists.
    menu.onSelection() += [this, alive = aliveToken()](int id) {
        if (alive.expired()) return;
        setOpen(false);
        itemSelected(id);
        on_item_selected_.callback(id);
    };
    menu.onCancel() += [this, alive = aliveToken()] {
        if (alive.expired()) return;
        cancelled_at_ = std::chrono::steady_clock::now();
        setOpen(false);
    };

    // The drawn menu copies this button's theme, so this opens it as wide as the button.
    set(PopupMenu::kMinWidth, width());

    setOpen(true);
    if (native_)
        menu.showNative(this, {0.0f, height()});
    else
        menu.show(this);
}

void PopupMenuButton::drawBackground(applause::Canvas& canvas, float hover) {
    Button::drawBackground(canvas, hover);
    if (!open_ || role() == Role::Flat) return;

    canvas.setColor(color(*this, kOpen));
    canvas.roundedRectangleBorder(0, 0, width(), height(), value(*this, kRounding), value(*this, kBorderWidth));
}

}  // namespace applause
