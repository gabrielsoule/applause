#pragma once

#include <visage_graphics/text.h>
#include <visage_graphics/theme.h>
#include <visage_ui/frame.h>

#include <string>

namespace applause {

class Panel : public visage::Frame {
public:
    VISAGE_THEME_DEFINE_COLOR(ApplausePanelBackground);
    VISAGE_THEME_DEFINE_COLOR(ApplausePanelBorder);
    VISAGE_THEME_DEFINE_COLOR(ApplausePanelTitleText);
    VISAGE_THEME_DEFINE_VALUE(ApplausePanelRounding);
    VISAGE_THEME_DEFINE_VALUE(ApplausePanelBorderWidth);
    VISAGE_THEME_DEFINE_VALUE(ApplausePanelTitleHeight);
    VISAGE_THEME_DEFINE_VALUE(ApplausePanelContentMargin);

    explicit Panel(const std::string& title);

    void draw(visage::Canvas& canvas) override;
    void resized() override;

    visage::Frame& content() { return content_; }

private:
    visage::Text title_;
    visage::Frame content_;
};

}  // namespace applause
