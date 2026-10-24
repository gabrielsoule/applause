#pragma once

#include <applause/ui/ApplauseUI.h>
#include <embedded/applause_fonts.h>

#include <string>

// A small label in a tinted pill. It shows how a component of your own takes part in theming: it declares its tokens
// next to itself, gives them defaults, and reads them while drawing. Nothing else is needed for themes, the inspector,
// or per-item overrides to reach it.
class Badge final : public applause::Frame {
public:
    // The badge's main color. Its fill and border are this color at lower opacities, so setting this one token
    // restyles the whole badge. By default it follows the shared accent.
    static inline const applause::ColorToken kColor{"badge.color", applause::theme::kAccent};
    static inline const applause::ValueToken kFillOpacity{"badge.fillOpacity", 0.15f};
    static inline const applause::ValueToken kBorderOpacity{"badge.borderOpacity", 0.5f};
    static inline const applause::ValueToken kRounding{"badge.rounding", 6.0f};

    explicit Badge(const std::string& text) : text_(text, applause::Font(12, applause::fonts::Barlow_Medium_ttf)) {}

    void draw(applause::Canvas& canvas) override {
        const float rounding = applause::value(*this, kRounding);
        const applause::Color base = applause::color(*this, kColor);
        canvas.setColor(base.withMultipliedAlpha(applause::value(*this, kFillOpacity)));
        canvas.roundedRectangle(0, 0, width(), height(), rounding);
        canvas.setColor(base.withMultipliedAlpha(applause::value(*this, kBorderOpacity)));
        canvas.roundedRectangleBorder(0, 0, width(), height(), rounding, 1.0f);
        canvas.setColor(base);
        canvas.text(&text_, 0, 0, width(), height());
    }

private:
    applause::Text text_;
};
