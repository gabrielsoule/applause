/*
 * Adapted from the Visage button. This is effectively a re-implementation.
 */

#pragma once

#include <applause/ui/ApplauseUI.h>

#include <functional>

namespace applause {
class Button : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonShadow);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonGlow);

    // Shared background gradient (normal, hover)
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonBackgroundTop);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonBackgroundBottom);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonBackgroundTopHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonBackgroundBottomHover);

    // Shared text colors
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonText);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonTextHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonTextPressed);

    // Shared border colors
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonBorder);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonBorderHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseButtonBorderPressed);

    // Shared values
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseButtonRounding);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseButtonHoverRoundingMult);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseButtonBorderWidth);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseButtonBorderWidthHover);
    APPLAUSE_THEME_DEFINE_VALUE(ApplauseButtonBorderWidthPressed);

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

    void draw(applause::Canvas& canvas) final;

    virtual void draw(applause::Canvas& canvas, float hover_amount) {}

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

    void setUndoSetupFunction(std::function<void()> undo_setup_function) {
        undo_setup_function_ = std::move(undo_setup_function);
    }

    std::function<void()> undoSetupFunction() { return undo_setup_function_; }
    bool wasAltClicked() const { return alt_clicked_; }

private:
    applause::CallbackList<void(Button*, bool)> on_toggle_;
    applause::Animation<float> hover_amount_;
    std::function<void()> undo_setup_function_ = nullptr;

    bool active_ = true;
    bool toggle_on_mouse_down_ = false;
    bool set_pointer_cursor_ = true;
    bool alt_clicked_ = false;
    bool pressed_ = false;

    APPLAUSE_LEAK_CHECKER(Button)
};

class UiButton : public Button {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseActionButtonBackgroundTop);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseActionButtonBackgroundBottom);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseActionButtonBackgroundTopHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseActionButtonBackgroundBottomHover);

    APPLAUSE_THEME_DEFINE_COLOR(ApplauseActionButtonBorder);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseActionButtonBorderHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseActionButtonBorderPressed);

    explicit UiButton(const std::string& text);

    UiButton() : UiButton("") {}

    explicit UiButton(const std::string& text, const applause::Font& font);

    virtual void drawBackground(applause::Canvas& canvas, float hover_amount);
    void draw(applause::Canvas& canvas, float hover_amount) override;

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

    void draw(applause::Canvas& canvas, float hover_amount) override;

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

class ButtonChangeAction;

class ToggleButton : public Button {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseToggleButtonDisabled);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseToggleButtonOff);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseToggleButtonOffHover);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseToggleButtonOn);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseToggleButtonOnHover);

    ToggleButton() = default;

    explicit ToggleButton(const std::string& name) : Button(name) {}

    bool toggle() override;

    void setToggled(bool toggled) override {
        toggled_ = toggled;
        toggleValueChanged();
        redraw();
    }

    virtual void toggleValueChanged() {}

    void setToggledAndNotify(bool toggled) override {
        toggled_ = toggled;
        redraw();
        notify(toggled);
    }

    bool toggled() const { return toggled_; }
    void setUndoable(bool undoable) { undoable_ = undoable; }

private:
    bool toggled_ = false;
    bool undoable_ = true;

    APPLAUSE_LEAK_CHECKER(ToggleButton)
};

class ButtonChangeAction : public applause::UndoableAction {
public:
    ButtonChangeAction(ToggleButton* button, bool toggled_on) : button_(button), toggled_on_(toggled_on) {}

    void undo() override { button_->setToggledAndNotify(!toggled_on_); }
    void redo() override { button_->setToggledAndNotify(toggled_on_); }

private:
    ToggleButton* button_ = nullptr;
    bool toggled_on_ = false;
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

    void draw(applause::Canvas& canvas, float hover_amount) override;

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
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseToggleTextButtonTextOn);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseToggleTextButtonTextOnHover);

    APPLAUSE_THEME_DEFINE_COLOR(ApplauseToggleTextButtonGlow);

    APPLAUSE_THEME_DEFINE_COLOR(ApplauseToggleTextButtonBorderOn);
    APPLAUSE_THEME_DEFINE_COLOR(ApplauseToggleTextButtonBorderOnHover);

    explicit ToggleTextButton(const std::string& name);
    explicit ToggleTextButton(const std::string& name, const applause::Font& font);

    virtual void drawBackground(applause::Canvas& canvas, float hover_amount);
    void draw(applause::Canvas& canvas, float hover_amount) override;

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
