#include "Panel.h"

#include <applause/ui/ApplauseUI.h>

#include <embedded/applause_fonts.h>

namespace applause {

APPLAUSE_THEME_IMPLEMENT_COLOR(Panel, ApplausePanelBackground, 0xff1a1a1f);
APPLAUSE_THEME_IMPLEMENT_COLOR(Panel, ApplausePanelBorder, 0xff2e2e34);
APPLAUSE_THEME_IMPLEMENT_COLOR(Panel, ApplausePanelTitleText, 0xffcccccc);
APPLAUSE_THEME_IMPLEMENT_VALUE(Panel, ApplausePanelRounding, 6.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Panel, ApplausePanelBorderWidth, 1.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Panel, ApplausePanelTitleHeight, 40.0f);
APPLAUSE_THEME_IMPLEMENT_VALUE(Panel, ApplausePanelContentMargin, 4.0f);

Panel::Panel(const std::string& title) : title_(title, applause::Font(15, applause::fonts::Jost_Medium_ttf)) {
    addChild(&content_);
}

void Panel::draw(applause::Canvas& canvas) {
    float w = width();
    float h = height();
    float rounding = canvas.value(ApplausePanelRounding);
    float border_width = canvas.value(ApplausePanelBorderWidth);
    float title_h = canvas.value(ApplausePanelTitleHeight);

    canvas.setColor(ApplausePanelBackground);
    canvas.roundedRectangle(0, 0, w, h, rounding);

    canvas.setColor(ApplausePanelBorder);
    canvas.roundedRectangleBorder(0, 0, w, h, rounding, border_width);

    canvas.setColor(ApplausePanelTitleText);
    canvas.text(&title_, 0, 0, w, title_h);
}

void Panel::resized() {
    float title_h = paletteValue(ApplausePanelTitleHeight);
    float margin = paletteValue(ApplausePanelContentMargin);
    content_.setBounds(margin, title_h, width() - 2 * margin, height() - title_h - margin);
}

}  // namespace applause
