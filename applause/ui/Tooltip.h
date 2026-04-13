#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <visage_graphics/animation.h>
#include <visage_graphics/text.h>
#include <visage_graphics/theme.h>
#include <visage_ui/events.h>
#include <visage_ui/frame.h>

namespace applause {

class TooltipDisplay : public visage::Frame, public visage::EventTimer {
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

    VISAGE_THEME_DEFINE_COLOR(TooltipBackground);
    VISAGE_THEME_DEFINE_COLOR(TooltipText);
    VISAGE_THEME_DEFINE_COLOR(TooltipBorder);

    TooltipDisplay();

    void showAt(const std::string& text, visage::Point window_pos);
    void hide();

    void draw(visage::Canvas& canvas) override;
    void timerCallback() override;

private:
    enum class State { kIdle, kWaiting, kFadingIn, kVisible };

    void applyContent(const std::string& text, visage::Point window_pos);

    State state_ = State::kIdle;
    visage::Animation<float> opacity_{kFadeInMs, visage::Animation<float>::kEaseOut};
    visage::Text text_;
    std::string pending_text_;
    visage::Point pending_pos_;
    visage::Bounds tooltip_bounds_;
};

void setTooltip(visage::Frame& frame, std::string text);
void removeTooltip(visage::Frame& frame);

}  // namespace applause
