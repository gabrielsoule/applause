/*
 * Adapted from the Visage button. This is effectively a re-implementation.
 */

#pragma once

#include <applause/ui/ApplauseUI.h>

#include <functional>

namespace applause {
class RadioGroup;
class Button : public applause::Frame {
public:
    static inline const ColorToken kShadow{"button.shadow", 0x88000000};
    static inline const ColorToken kGlow{"button.glow", theme::kAccent};

    // Shared background gradient (normal, hover)
    static inline const ColorToken kBackgroundTop{"button.backgroundTop", 0xff333333};
    static inline const ColorToken kBackgroundBottom{"button.backgroundBottom", 0xff222222};
    static inline const ColorToken kBackgroundTopHover{"button.backgroundTopHover", 0xff3e3e42};
    static inline const ColorToken kBackgroundBottomHover{"button.backgroundBottomHover", 0xff2e2e32};

    // Shared text colors
    static inline const ColorToken kText{"button.text", 0xffcccccc};
    static inline const ColorToken kTextHover{"button.textHover", 0xffdddddd};
    static inline const ColorToken kTextPressed{"button.textPressed", 0xffffffff};

    // Shared border colors
    static inline const ColorToken kBorder{"button.border", 0xff444444};
    static inline const ColorToken kBorderHover{"button.borderHover", 0xff5a5a60};
    static inline const ColorToken kBorderPressed{"button.borderPressed", 0xff555555};

    // Shared values
    static inline const ValueToken kGlowAmount{"button.glowAmount", 0.45f};
    static inline const ValueToken kRounding{"button.rounding", 7.0f};
    static inline const ValueToken kHoverRoundingMult{"button.hoverRoundingMult", 1.0f};
    static inline const ValueToken kBorderWidth{"button.borderWidth", 1.5f};
    static inline const ValueToken kBorderWidthHover{"button.borderWidthHover", kBorderWidth};
    static inline const ValueToken kBorderWidthPressed{"button.borderWidthPressed", kBorderWidth};

    Button() {
        hover_amount_.setTargetValue(1.0f);
        hover_amount_.setAnimationTime(200);
    }

    explicit Button(const std::string& name) : Frame(name) {
        hover_amount_.setTargetValue(1.0f);
        hover_amount_.setAnimationTime(200);
    }

    auto& onToggle() { return on_toggle_; }

    virtual bool toggle() { return false; }

    virtual void setToggled(bool toggled) {}

    virtual void setToggledAndNotify(bool toggled) {
        if (toggled) notify(false);
    }

    void notify(bool on) { on_toggle_.callback(this, on); }

    void draw(applause::Canvas& canvas) override;

    void mouseEnter(const applause::MouseEvent& e) override;
    void mouseExit(const applause::MouseEvent& e) override;
    void mouseDown(const applause::MouseEvent& e) override;
    void mouseUp(const applause::MouseEvent& e) override;

    void setToggleOnMouseDown(bool mouse_down) { toggle_on_mouse_down_ = mouse_down; }
    float hoverAmount() const { return hover_amount_.value(); }
    void setActive(bool active) {
        active_ = active;
        redraw();
    }
    bool isActive() const { return active_; }
    bool isPressed() const { return pressed_; }

    bool wasAltClicked() const { return alt_clicked_; }

protected:
    applause::Animation<float> hover_amount_;

private:
    applause::CallbackList<void(Button*, bool)> on_toggle_;

    bool active_ = true;
    bool toggle_on_mouse_down_ = false;
    bool set_pointer_cursor_ = true;
    bool alt_clicked_ = false;
    bool pressed_ = false;

    APPLAUSE_LEAK_CHECKER(Button)
};

class UiButton : public Button {
public:
    static inline const ColorToken kActionBackgroundTop{"uiButton.actionBackgroundTop", theme::kAccent};
    static inline const ColorToken kActionBackgroundBottom{"uiButton.actionBackgroundBottom", kActionBackgroundTop};
    static inline const ColorToken kActionBackgroundTopHover{"uiButton.actionBackgroundTopHover", kActionBackgroundTop};
    static inline const ColorToken kActionBackgroundBottomHover{"uiButton.actionBackgroundBottomHover",
                                                                kActionBackgroundBottom};

    static inline const ColorToken kActionBorder{"uiButton.actionBorder", theme::kAccent};
    static inline const ColorToken kActionBorderHover{"uiButton.actionBorderHover", kActionBorder};
    static inline const ColorToken kActionBorderPressed{"uiButton.actionBorderPressed", 0xffffffff};

    explicit UiButton(const std::string& text);

    UiButton() : UiButton("") {}

    explicit UiButton(const std::string& text, const applause::Font& font);

    virtual void drawBackground(applause::Canvas& canvas, float hover_amount);
    void draw(applause::Canvas& canvas) override;

    void setFont(const applause::Font& font) {
        text_.setFont(font);
        redraw();
    }

    void setActionButton(bool action = true) {
        action_ = action;
        redraw();
    }

    void setText(const std::string& text) {
        text_.setText(text);
        redraw();
    }

    void drawBorderWhenInactive(bool border) { border_when_inactive_ = border; }

private:
    applause::Text text_;
    bool action_ = false;
    bool border_when_inactive_ = true;
};

class IconButton : public Button {
public:
    static constexpr float kDefaultShadowRadius = 3.0f;

    explicit IconButton(bool shadow = false) { initSettings(shadow); }

    explicit IconButton(const applause::Svg& icon, bool shadow = false) {
        setIcon(icon);
        initSettings(shadow);
    }

    explicit IconButton(const applause::EmbeddedFile& icon_file, bool shadow = false) {
        setIcon(icon_file);
        initSettings(shadow);
    }

    IconButton(const unsigned char* svg, int svg_size, bool shadow = false) {
        setIcon(svg, svg_size);
        initSettings(shadow);
    }

    void setIcon(const applause::EmbeddedFile& icon_file) { setIcon({icon_file.data, icon_file.size}); }

    void setIcon(const unsigned char* svg, int svg_size) { setIcon({svg, svg_size}); }

    void setIcon(const applause::Svg& icon) {
        icon_.load(icon);
        shadow_.load(icon);
    }

    void draw(applause::Canvas& canvas) override;

    void resized() override {
        Button::resized();
        icon_.setBounds(localBounds());
        shadow_.setBounds(localBounds());
    }

    void setShadowRadius(const applause::Dimension& radius) {
        shadow_radius_ = radius;
        computeShadowRadius();
    }

    void setMargin(const applause::Dimension& margin) {
        icon_.setMargin(margin);
        shadow_.setMargin(margin);
    }

private:
    void initSettings(bool shadow) {
        addChild(shadow_, shadow);
        shadow_.setIgnoresMouseEvents(true, false);

        addChild(icon_);
        icon_.setIgnoresMouseEvents(true, false);
        if (shadow) setShadowRadius(kDefaultShadowRadius);
    }

    void computeShadowRadius() {
        float r = shadow_radius_.compute(dpiScale(), nativeWidth(), nativeHeight(), 0.0f) / dpiScale();
        shadow_.setVisible(r > 0.0f);
        shadow_.setBlurRadius(r);
    }

    applause::SvgFrame icon_;
    applause::SvgFrame shadow_;

    applause::Dimension shadow_radius_;
};

class ToggleButton : public Button {
public:
    static inline const ColorToken kDisabled{"toggleButton.disabled", 0xff4c4f52};
    static inline const ColorToken kOff{"toggleButton.off", 0xff848789};
    static inline const ColorToken kOffHover{"toggleButton.offHover", 0xffaaacad};
    static inline const ColorToken kOn{"toggleButton.on", theme::kAccent};
    static inline const ColorToken kOnHover{"toggleButton.onHover", kOn};

    ToggleButton() = default;

    explicit ToggleButton(const std::string& name) : Button(name) {}

    ~ToggleButton() override;

    // Group membership refers to this button's stable identity.
    ToggleButton(const ToggleButton&) = delete;
    ToggleButton& operator=(const ToggleButton&) = delete;
    ToggleButton(ToggleButton&&) = delete;
    ToggleButton& operator=(ToggleButton&&) = delete;

    bool toggle() override;

    void setToggled(bool toggled) override;

    virtual void toggleValueChanged() {}

    void setToggledAndNotify(bool toggled) override;

    bool toggled() const { return toggled_; }

protected:
    /// Called after a real state change. Notifying changes include user activation and notifying setters.
    virtual void toggleStateChanged(bool notify) {}
    virtual bool allowsRadioGroup() const { return true; }

private:
    friend class RadioGroup;

    void dispatchToggleStateChanged(bool notify);
    void changeToggleState(bool toggled, bool notify);

    RadioGroup* radio_group_ = nullptr;
    bool toggled_ = false;

    APPLAUSE_LEAK_CHECKER(ToggleButton)
};

class ToggleIconButton : public ToggleButton {
public:
    static constexpr float kDefaultShadowRadius = 3.0f;

    explicit ToggleIconButton(const applause::Svg& icon, bool shadow = false) : ToggleButton() {
        setIcon(icon);
        initSettings(shadow);
    }

    ToggleIconButton(const std::string& name, const applause::Svg& icon, bool shadow = false) : ToggleButton(name) {
        setIcon(icon);
        initSettings(shadow);
    }

    ToggleIconButton(const unsigned char* svg, int svg_size, bool shadow = false) : ToggleButton() {
        setIcon({svg, svg_size});
        initSettings(shadow);
    }

    ToggleIconButton(const std::string& name, const unsigned char* svg, int svg_size, bool shadow = false) :
        ToggleButton(name) {
        setIcon({svg, svg_size});
        initSettings(shadow);
    }

    void setIcon(const applause::Svg& icon) {
        shadow_.load(icon);
        icon_.load(icon);
    }

    void draw(applause::Canvas& canvas) override;

    void resized() override {
        ToggleButton::resized();
        icon_.setBounds(localBounds());
        shadow_.setBounds(localBounds());
    }

    void setShadowRadius(const applause::Dimension& radius) {
        shadow_radius_ = radius;
        computeShadowRadius();
    }

    void setMargin(const applause::Dimension& margin) {
        icon_.setMargin(margin);
        shadow_.setMargin(margin);
    }

private:
    void initSettings(bool shadow) {
        addChild(shadow_, shadow);
        shadow_.setIgnoresMouseEvents(true, false);

        addChild(icon_);
        icon_.setIgnoresMouseEvents(true, false);
        if (shadow) setShadowRadius(kDefaultShadowRadius);
    }

    void computeShadowRadius() {
        float r = shadow_radius_.compute(dpiScale(), nativeWidth(), nativeHeight(), 0.0f) / dpiScale();
        shadow_.setVisible(r > 0.0f);
        shadow_.setBlurRadius(r);
    }

    applause::SvgFrame icon_;
    applause::SvgFrame shadow_;

    applause::Dimension shadow_radius_;
};

class ToggleTextButton : public ToggleButton {
public:
    static inline const ColorToken kTextOn{"toggleTextButton.textOn", kOn, 0.87f};
    static inline const ColorToken kTextOnHover{"toggleTextButton.textOnHover", kOnHover};

    static inline const ColorToken kGlowOn{"toggleTextButton.glowOn", theme::kAccent, 0.21f};
    // Fraction of the toggled glow's alpha kept at the center, so it doesn't fall off to nothing.
    static inline const ValueToken kGlowOnCenterAmount{"toggleTextButton.glowOnCenterAmount", 0.0f};

    static inline const ColorToken kBorderOn{"toggleTextButton.borderOn", kOn, 0.73f};
    static inline const ColorToken kBorderOnHover{"toggleTextButton.borderOnHover", kOnHover, 0.87f};

    explicit ToggleTextButton(const std::string& name);
    explicit ToggleTextButton(const std::string& name, const applause::Font& font);

    virtual void drawBackground(applause::Canvas& canvas, float hover_amount);
    void draw(applause::Canvas& canvas) override;

    void setFont(const applause::Font& font) {
        text_.setFont(font);
        redraw();
    }

    void setText(const std::string& text) { text_.setText(text); }
    void setDrawBackground(bool draw_background) { draw_background_ = draw_background; }

private:
    bool draw_background_ = true;
    applause::Text text_;
};
class NativePopupMenu;

class PopupMenuButton : public UiButton {
public:
    explicit PopupMenuButton(const std::string& default_text = "--");

    std::function<void(NativePopupMenu&)> on_build_menu_;
    applause::CallbackList<void(int)> on_item_selected_;

private:
    void showPopup();
};

}  // namespace applause
