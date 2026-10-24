/*
 * Adapted from the Visage button. This is effectively a re-implementation.
 */

#pragma once

#include <applause/ui/ApplauseUI.h>

#include <functional>
#include <memory>

namespace applause {
class NativePopupMenu;
class RadioGroup;

// A button that shows text, an icon, or both. A checkable button turns on when clicked and off when clicked again.
// Subclasses change the look by overriding drawBackground() and drawContent(), or draw() to replace all of it.
class Button : public applause::Frame {
public:
    // Primary marks a view's main action and fills with the accent color. Flat draws no fill or border, for
    // toolbars, icons, and links.
    enum class Role { Normal, Primary, Flat };

    static inline const ColorToken kBackground{"button.background", 0xff333333};
    static inline const ColorToken kBorder{"button.border", 0xff444444};
    static inline const ColorToken kText{"button.text", 0xffcccccc};
    static inline const ColorToken kTextHover{"button.textHover", 0xffdddddd};
    static inline const ColorToken kTextPressed{"button.textPressed", 0xffffffff};
    static inline const ColorToken kGlow{"button.glow", theme::kAccent};
    // A checked button's content and border, and the glow inside it.
    static inline const ColorToken kOn{"button.on", theme::kAccent};
    // The outline of an unchecked checkbox or radio button.
    static inline const ColorToken kOff{"button.off", 0xff848789};
    static inline const ColorToken kOffHover{"button.offHover", 0xffaaacad};
    // A Primary button's fill, and its content on top of it.
    static inline const ColorToken kPrimary{"button.primary", theme::kAccent};
    static inline const ColorToken kPrimaryText{"button.primaryText", 0xffffffff};

    static inline const ValueToken kRounding{"button.rounding", 7.0f};
    static inline const ValueToken kBorderWidth{"button.borderWidth", 1.5f};
    static inline const ValueToken kGlowAmount{"button.glowAmount", 0.45f};
    // kOn's opacity in the glow inside a checked button.
    static inline const ValueToken kOnGlowOpacity{"button.onGlowOpacity", 0.21f};
    // How much darker the bottom of the fill is than its top.
    static inline const ValueToken kShade{"button.shade", 0.33f};
    // How much lighter the fill and border get on hover.
    static inline const ValueToken kHoverLighten{"button.hoverLighten", 0.08f};
    // Space on each side of the content, used by preferredWidth().
    static inline const ValueToken kPadding{"button.padding", 10.0f};
    static inline const ValueToken kIconSize{"button.iconSize", 16.0f};
    static inline const ValueToken kIconGap{"button.iconGap", 6.0f};

    explicit Button(std::string text = {});
    explicit Button(const Svg& icon, std::string text = {});
    ~Button() override;

    const String& text() const { return text_.text(); }
    void setText(std::string text);
    const Font& font() const { return text_.font(); }
    void setFont(const Font& font);
    void setIcon(const Svg& icon);

    Role role() const { return role_; }
    void setRole(Role role);
    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled);

    bool isCheckable() const { return checkable_; }
    void setCheckable(bool checkable);
    bool isChecked() const { return checked_; }
    // Doesn't run onToggle().
    void setChecked(bool checked);
    void setCheckedAndNotify(bool checked);

    // Runs on every click, after a checkable button turns on or off.
    auto& onClick() { return on_click_; }
    // Runs when the button turns on or off because of a click or a notifying setter, on it or on its radio group.
    auto& onToggle() { return on_toggle_; }
    // Does what a mouse click does.
    void click();
    void setTriggeredOnMouseDown(bool triggered) { triggered_on_mouse_down_ = triggered; }

    bool isPressed() const { return pressed_; }
    // The width that fits the content and padding.
    float preferredWidth() const;

    // Draws the background, then the content clipped to contentBounds().
    void draw(applause::Canvas& canvas) override;
    void resized() override { placeIcon(); }

    void mouseEnter(const applause::MouseEvent& e) override;
    void mouseExit(const applause::MouseEvent& e) override;
    void mouseDown(const applause::MouseEvent& e) override;
    void mouseUp(const applause::MouseEvent& e) override;

protected:
    // Called after the button turns on or off, before onToggle() runs. `notify` is false for silent changes.
    virtual void checkedChanged(bool notify) {}
    virtual bool allowsRadioGroup() const { return true; }

    // Advances the hover animation and asks for another frame while it runs. Call it once per draw. The result runs
    // from 0 to 1 as the mouse enters, and stays 0 while the button is disabled.
    float updateHover();

    // `hover` is the result of updateHover().
    virtual void drawBackground(applause::Canvas& canvas, float hover);
    virtual void drawContent(applause::Canvas& canvas, float hover);
    // Where drawContent() draws; drawing outside it is clipped.
    virtual Bounds contentBounds() const { return localBounds(); }

private:
    friend class RadioGroup;

    void changeChecked(bool checked, bool notify);
    void dispatchCheckedChanged(bool notify);
    // Centers the icon and the text together, the icon first.
    void placeIcon();

    applause::Text text_;
    std::unique_ptr<applause::SvgFrame> icon_;
    applause::Animation<float> hover_amount_;
    applause::CallbackList<void()> on_click_;
    applause::CallbackList<void(bool)> on_toggle_;
    RadioGroup* radio_group_ = nullptr;
    Role role_ = Role::Normal;
    bool enabled_ = true;
    bool checkable_ = false;
    bool checked_ = false;
    bool pressed_ = false;
    bool triggered_on_mouse_down_ = false;

    APPLAUSE_LEAK_CHECKER(Button)
};

// A button that opens a native menu when clicked.
class PopupMenuButton : public Button {
public:
    explicit PopupMenuButton(std::string text = "--");

    // Fills the menu each time it opens.
    auto& onBuildMenu() { return on_build_menu_; }
    auto& onItemSelected() { return on_item_selected_; }

private:
    void showPopup();

    std::function<void(NativePopupMenu&)> on_build_menu_;
    applause::CallbackList<void(int)> on_item_selected_;
};

}  // namespace applause
