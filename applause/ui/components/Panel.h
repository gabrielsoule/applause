#pragma once

#include <applause/ui/ApplauseUI.h>

#include <string>

namespace applause {

class Panel : public applause::Frame {
public:
    APPLAUSE_THEME_DEFINE_COLOR(ApplausePanelBackground);
    APPLAUSE_THEME_DEFINE_COLOR(ApplausePanelBorder);
    APPLAUSE_THEME_DEFINE_COLOR(ApplausePanelTitleText);
    APPLAUSE_THEME_DEFINE_VALUE(ApplausePanelRounding);
    APPLAUSE_THEME_DEFINE_VALUE(ApplausePanelBorderWidth);
    APPLAUSE_THEME_DEFINE_VALUE(ApplausePanelTitleHeight);
    APPLAUSE_THEME_DEFINE_VALUE(ApplausePanelContentMargin);

// Titleless variant. the content frame fills the full panel (minus the standard inner margin).
    Panel();
    explicit Panel(const std::string& title);

    void draw(applause::Canvas& canvas) override;
    void resized() override;

    applause::Frame& content() { return content_; }

private:
    applause::Text title_;
    applause::Frame content_;
    bool has_title_;
};

}  // namespace applause
