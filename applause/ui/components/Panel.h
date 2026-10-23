#pragma once

#include <applause/ui/ApplauseUI.h>

#include <string>

namespace applause {

/**
 * A simple Panel with a border, fill color, and optional title text. Useful for grouping related controls.
 */
class Panel : public applause::Frame {
public:
    static inline const ColorToken kBackground{"panel.background", 0xff1a1a1f};
    static inline const ColorToken kBorder{"panel.border", 0xff2e2e34};
    static inline const ColorToken kTitleText{"panel.titleText", 0xffcccccc};
    static inline const ValueToken kRounding{"panel.rounding", 6.0f};
    static inline const ValueToken kBorderWidth{"panel.borderWidth", 1.0f};
    static inline const ValueToken kTitleHeight{"panel.titleHeight", 40.0f};
    static inline const ValueToken kContentMargin{"panel.contentMargin", 4.0f};

    // Titleless variant. the content frame fills the full panel (minus the standard inner margin).
    Panel();
    explicit Panel(const std::string& title);

    void draw(applause::Canvas& canvas) override;
    void resized() override;

    void setFont(const applause::Font& font) {
        title_.setFont(font);
        redraw();
    }

    applause::Frame& content() { return content_; }

private:
    applause::Text title_;
    applause::Frame content_;
    bool has_title_;
};

}  // namespace applause
