#pragma once

#include <applause/ui/ApplauseUI.h>
#include <embedded/applause_fonts.h>

#include <string>

// A small label in a tinted pill. It shows how a component of your own takes part in theming: it declares its tokens
// next to itself, gives them defaults, and reads them while drawing. Nothing else is needed for themes, the inspector,
// or per-item overrides to reach it.
class Badge final : public applause::Frame {
public:
    // The badge's main color. Its fill and border follow it at lower alphas, so setting this one token restyles the
    // whole badge. By default it follows the shared accent.
    static inline const applause::ColorToken kColor{"badge.color", applause::theme::kAccent};
    static inline const applause::ColorToken kFill{"badge.fill", kColor, 0.15f};
    static inline const applause::ColorToken kBorder{"badge.border", kColor, 0.5f};
    static inline const applause::ValueToken kRounding{"badge.rounding", 6.0f};

    explicit Badge(const std::string& text) : text_(text, applause::Font(12, applause::fonts::Barlow_Medium_ttf)) {}

    void draw(applause::Canvas& canvas) override {
        const float rounding = applause::value(*this, kRounding);
        canvas.setColor(applause::color(*this, kFill));
        canvas.roundedRectangle(0, 0, width(), height(), rounding);
        canvas.setColor(applause::color(*this, kBorder));
        canvas.roundedRectangleBorder(0, 0, width(), height(), rounding, 1.0f);
        canvas.setColor(applause::color(*this, kColor));
        canvas.text(&text_, 0, 0, width(), height());
    }

private:
    applause::Text text_;
};
