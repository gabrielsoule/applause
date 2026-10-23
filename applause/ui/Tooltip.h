#pragma once

#include <applause/ui/ApplauseUI.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace applause {

class TooltipDisplay : public applause::Frame, public applause::EventTimer {
public:
    static constexpr float kCursorOffsetX = 8.0f;
    static constexpr float kCursorOffsetY = 16.0f;
    static constexpr float kPaddingX = 8.0f;
    static constexpr float kPaddingY = 5.0f;
    static constexpr float kCornerRadius = 4.0f;
    static constexpr float kBorderWidth = 1.0f;
    static constexpr float kFontSize = 11.0f;
    static constexpr int kHoverDelayMs = 400;
    static constexpr int kFadeInMs = 120;
    static constexpr int kAnimFrameMs = 16;

    static inline const ColorToken kBackground{"tooltip.background", 0xee1e1e24};
    static inline const ColorToken kText{"tooltip.text", 0xffcccccc};
    static inline const ColorToken kBorder{"tooltip.border", 0xff3a3a42};

    TooltipDisplay();

    void showAt(const std::string& text, applause::Point window_pos);
    void hide();

    void setFont(const applause::Font& font);

    void draw(applause::Canvas& canvas) override;
    void timerCallback() override;

private:
    enum class State { kIdle, kWaiting, kFadingIn, kVisible };

    void applyContent(const std::string& text, applause::Point window_pos);

    State state_ = State::kIdle;
    applause::Animation<float> opacity_{kFadeInMs, applause::Animation<float>::kEaseOut};
    applause::Text text_;
    std::string pending_text_;
    applause::Point pending_pos_;
    applause::Bounds tooltip_bounds_;
};

void setTooltip(applause::Frame& frame, std::string text);
void removeTooltip(applause::Frame& frame);

}  // namespace applause
