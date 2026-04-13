#include "Tooltip.h"

#include "ApplauseEditor.h"

#include <embedded/applause_fonts.h>

namespace applause {

VISAGE_THEME_IMPLEMENT_COLOR(TooltipDisplay, TooltipBackground, 0xee1e1e24);
VISAGE_THEME_IMPLEMENT_COLOR(TooltipDisplay, TooltipText, 0xffcccccc);
VISAGE_THEME_IMPLEMENT_COLOR(TooltipDisplay, TooltipBorder, 0xff3a3a42);

namespace {

struct TooltipBinding {
    std::string text;
    bool enabled = false;
    bool hooked = false;
};

std::unordered_map<visage::Frame*, TooltipBinding> tooltip_bindings;

}  // namespace

TooltipDisplay::TooltipDisplay() : text_("", visage::Font(kFontSize, applause::fonts::Jost_Regular_ttf)) {
    setIgnoresMouseEvents(true, false);
    opacity_.setSourceValue(0.0f);
    opacity_.setTargetValue(1.0f);
}

void TooltipDisplay::applyContent(const std::string& text, visage::Point window_pos) {
    text_.setText(text);

    visage::Font font = text_.font().withDpiScale(dpiScale());
    visage::String vs(text);
    float text_width = font.stringWidth(vs.toUtf32().c_str(), vs.toUtf32().size());
    float tooltip_width = text_width + kPaddingX * 2.0f;
    float tooltip_height = font.lineHeight() + kPaddingY * 2.0f;

    float tooltip_x = window_pos.x + kCursorOffsetX;
    float tooltip_y = window_pos.y + kCursorOffsetY;

    if (tooltip_y + tooltip_height > height()) tooltip_y = window_pos.y - tooltip_height - 4.0f;

    if (tooltip_x + tooltip_width > width()) tooltip_x = width() - tooltip_width;
    if (tooltip_x < 0.0f) tooltip_x = 0.0f;
    if (tooltip_y < 0.0f) tooltip_y = 0.0f;

    tooltip_bounds_ = {tooltip_x, tooltip_y, tooltip_width, tooltip_height};
}

void TooltipDisplay::showAt(const std::string& text, visage::Point window_pos) {
    if (state_ == State::kVisible || state_ == State::kFadingIn) {
        applyContent(text, window_pos);
        redraw();
        return;
    }

    pending_text_ = text;
    pending_pos_ = window_pos;
    state_ = State::kWaiting;
    startTimer(kHoverDelayMs);
}

void TooltipDisplay::hide() {
    if (state_ == State::kIdle) return;
    stopTimer();
    state_ = State::kIdle;
    opacity_.target(false, true);
    setAlphaTransparency(1.0f);
    redraw();
}

void TooltipDisplay::timerCallback() {
    if (state_ == State::kWaiting) {
        if (auto* editor = findParent<ApplauseEditor>()) {
            if (auto* window = editor->window()) {
                pending_pos_ = window->convertToLogical(window->lastWindowMousePosition());
            }
        }

        applyContent(pending_text_, pending_pos_);
        opacity_.target(true);
        setAlphaTransparency(0.0f);
        state_ = State::kFadingIn;
        startTimer(kAnimFrameMs);
        redraw();
        return;
    }

    if (state_ == State::kFadingIn) {
        float alpha = opacity_.update();
        setAlphaTransparency(alpha);
        if (!opacity_.isAnimating()) {
            state_ = State::kVisible;
            setAlphaTransparency(1.0f);
            stopTimer();
        }
    }
}

void TooltipDisplay::draw(visage::Canvas& canvas) {
    if (state_ == State::kIdle || state_ == State::kWaiting) return;

    const auto& bounds = tooltip_bounds_;

    canvas.setColor(TooltipBackground);
    canvas.roundedRectangle(bounds.x(), bounds.y(), bounds.width(), bounds.height(), kCornerRadius);

    canvas.setColor(TooltipBorder);
    canvas.roundedRectangleBorder(bounds.x(), bounds.y(), bounds.width(), bounds.height(), kCornerRadius,
                                  kBorderWidth);

    canvas.setColor(TooltipText);
    canvas.text(&text_, bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

void setTooltip(visage::Frame& frame, std::string text) {
    auto& binding = tooltip_bindings[&frame];
    binding.text = std::move(text);
    binding.enabled = true;

    if (binding.hooked) return;
    binding.hooked = true;

    visage::Frame* frame_ptr = &frame;

    frame.onMouseEnter() += [frame_ptr](const visage::MouseEvent& e) {
        auto it = tooltip_bindings.find(frame_ptr);
        if (it == tooltip_bindings.end() || !it->second.enabled) return;
        auto* editor = frame_ptr->findParent<applause::ApplauseEditor>();
        if (!editor) return;
        editor->tooltipDisplay().showAt(it->second.text, e.windowPosition());
    };

    frame.onMouseExit() += [frame_ptr](const visage::MouseEvent&) {
        auto* editor = frame_ptr->findParent<applause::ApplauseEditor>();
        if (!editor) return;
        editor->tooltipDisplay().hide();
    };

    frame.onMouseDown() += [frame_ptr](const visage::MouseEvent&) {
        auto* editor = frame_ptr->findParent<applause::ApplauseEditor>();
        if (!editor) return;
        editor->tooltipDisplay().hide();
    };
}

void removeTooltip(visage::Frame& frame) {
    auto it = tooltip_bindings.find(&frame);
    if (it == tooltip_bindings.end()) return;

    it->second.enabled = false;
    it->second.text.clear();
}

}  // namespace applause
