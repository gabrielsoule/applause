#pragma once

#include <applause/ui/components/Button.h>

#include <chrono>
#include <functional>
#include <string>

namespace applause {
class PopupMenu;

// A button that opens a menu below itself: the drawn, themed PopupMenu, as wide as the button or wider to fit its
// options, or after setNative(true) the operating system's menu. Linux has no native menu, so it always gets the drawn
// one. Dropdown adds a selection on top.
class PopupMenuButton : public Button {
public:
    // The border while the menu is open.
    static inline const ColorToken kOpen{"popupMenuButton.open", theme::kAccent};

    explicit PopupMenuButton(std::string text = "--");

    bool isNative() const { return native_; }
    // A drawn menu opens on press, so the press can be dragged onto an option. A native menu opens on release.
    void setNative(bool native);
    bool isOpen() const { return open_; }

    // Fills the menu each time it opens.
    auto& onBuildMenu() { return on_build_menu_; }
    auto& onItemSelected() { return on_item_selected_; }

protected:
    // Fills the menu before it opens. The default runs onBuildMenu().
    virtual void buildMenu(PopupMenu& menu);
    // Called with the picked option's ID, before onItemSelected() runs.
    virtual void itemSelected(int id) {}
    bool allowsRadioGroup() const override { return false; }
    void drawBackground(applause::Canvas& canvas, float hover) override;

private:
    void showPopup();
    void setOpen(bool open);

    std::function<void(PopupMenu&)> on_build_menu_;
    applause::CallbackList<void(int)> on_item_selected_;
    std::chrono::steady_clock::time_point cancelled_at_;
    bool native_ = false;
    bool open_ = false;
};

}  // namespace applause
