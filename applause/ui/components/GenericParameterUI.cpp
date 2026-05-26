#include "GenericParameterUI.h"

#include <applause/ui/ApplauseUI.h>

#include <embedded/applause_fonts.h>

#include <algorithm>

#include <applause/util/DebugHelpers.h>

using namespace applause::dimension;

namespace applause {
GenericParameterEntry::GenericParameterEntry(ParamInfo& paramInfo) : paramInfo_(paramInfo), paramSlider_(paramInfo) {
    addChild(&paramSlider_);
}

void GenericParameterEntry::draw(applause::Canvas& canvas) {
    // Calculate text area bounds (left side of the component)
    // Full width minus padding only on the right (between text and slider)
    float textX = 0;
    float textY = 0;
    float textWidth = labelWidth_ - kLabelPadding;
    float textHeight = height();

    // Draw the parameter name (right-aligned)
    const applause::Font font(13, applause::fonts::Jost_Regular_ttf);
    canvas.setColor(0xFFCCCCCC);
    canvas.text(paramInfo_.name, font, applause::Font::kRight, textX, textY, textWidth, textHeight);
}

void GenericParameterEntry::resized() {
    // Position the ParamSlider on the right side, leaving space for label on the left
    float sliderWidth = width() - labelWidth_;
    float sliderX = labelWidth_;

    paramSlider_.setBounds(sliderX, 0, sliderWidth, height());
}

void GenericParameterEntry::setLabelWidth(float labelWidth) {
    labelWidth_ = labelWidth;
    resized();
    redraw();
}

GenericParameterUI::GenericParameterUI() {
    // Configure scrollable vertical flex layout
    scrollableLayout().setFlex(true);
    scrollableLayout().setFlexRows(true);
    scrollableLayout().setFlexGap(kEntryGap);
    scrollableLayout().setPadding(kPadding);
    scrollableLayout().setFlexItemAlignment(applause::Layout::ItemAlignment::Stretch);
}

void GenericParameterUI::draw(applause::Canvas& canvas) {}

void GenericParameterUI::resized() {
    ScrollableFrame::resized();

    // Let flex layout compute positions first
    computeLayout();

    // Then get actual content height from computed layout
    // oddly enough there is not a function to get the width/height that a flex
    // layout would consume
    float contentHeight = 0;
    if (!entries_.empty()) {
        // Find the bottom-most child position
        for (auto& entry : entries_) {
            contentHeight = std::max(contentHeight, entry->bottom());
        }
        // Add bottom padding from layout
        contentHeight += scrollableLayout().paddingBottom().amount;
    }

    setScrollableHeight(contentHeight);
}

void GenericParameterUI::addParameter(ParamInfo& paramInfo) {
    LOG_DBG("Adding parameter {}", paramInfo.name);

    auto entry = std::make_unique<GenericParameterEntry>(paramInfo);
    entry->layout().setHeight(kEntryHeight);
    entry->layout().setFlexGrow(0.0f);
    entry->layout().setFlexShrink(0.0f);

    addScrolledChild(entry.get());
    entries_.push_back(std::move(entry));

    computeLayout();
    resized();
}
}  // namespace applause
