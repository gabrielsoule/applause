#include "Panel.h"

#include <applause/ui/ApplauseUI.h>

#include <embedded/applause_fonts.h>

namespace applause {

Panel::Panel() : has_title_(false) { addChild(&content_); }

Panel::Panel(const std::string& title) :
    title_(title, applause::Font(15, applause::fonts::Barlow_Medium_ttf)), has_title_(true) {
    addChild(&content_);
}

void Panel::draw(applause::Canvas& canvas) {
    float w = width();
    float h = height();
    float rounding = value(*this, kRounding);
    float border_width = value(*this, kBorderWidth);

    canvas.setColor(color(*this, kBackground));
    canvas.roundedRectangle(0, 0, w, h, rounding);

    canvas.setColor(color(*this, kBorder));
    canvas.roundedRectangleBorder(0, 0, w, h, rounding, border_width);

    if (has_title_) {
        float title_h = value(*this, kTitleHeight);
        canvas.setColor(color(*this, kTitleText));
        canvas.text(&title_, 0, 0, w, title_h);
    }
}

void Panel::resized() {
    const float margin = value(*this, kContentMargin);
    const float top = has_title_ ? value(*this, kTitleHeight) : margin;
    content_.setBounds(margin, top, width() - 2 * margin, height() - top - margin);
}

}  // namespace applause
