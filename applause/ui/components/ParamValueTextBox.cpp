#include "ParamValueTextBox.h"

#include <applause/ui/ApplauseUI.h>

#include <embedded/applause_fonts.h>

#include <cmath>
#include <iomanip>

#include <applause/util/DebugHelpers.h>

namespace applause {

ParamValueTextBox::ParamValueTextBox(ParamInfo& paramInfo) : param_info_(paramInfo) {
    text_editor_.setMultiLine(false);
    text_editor_.setJustification(applause::Font::kCenter);
    text_editor_.setFont(applause::Font(12, applause::fonts::Jost_Regular_ttf));
    text_editor_.setMargin(0, 0);

    addChild(&text_editor_);

    updateTextDisplay();

    // Lambda to commit the current text value (shared by Enter and focus lost)
    auto commitValue = [this]() {
        if (!is_editing_) return;

        // Try to parse the entered text
        auto parsed_value = param_info_.textToValue(text_editor_.text().toUtf8());

        if (parsed_value.has_value()) {
            // Valid value entered - apply it
            param_info_.setValueNotifyingHost(parsed_value.value());
        }

        // Update display to show formatted value
        updateTextDisplay();
        is_editing_ = false;
        param_info_.endGesture();
    };

    // Set up TextEditor callbacks
    text_editor_.onTextChange() += [this]() {
        if (!is_editing_) {
            // Save original value when editing starts
            original_value_ = param_info_.getValue();
            is_editing_ = true;
            param_info_.beginGesture();
        }
    };

    text_editor_.onEnterKey() += commitValue;

    text_editor_.onFocusChange() += [this, commitValue](bool is_focused, bool was_clicked) {
        if (!is_focused) {
            commitValue();
        }
    };

    text_editor_.onEscapeKey() += [this]() {
        // Restore original value
        param_info_.setValueNotifyingHost(original_value_);

        // Update display to show original value
        updateTextDisplay();
        is_editing_ = false;
        param_info_.endGesture();
    };

    // Connect to parameter changes from the host
    param_connection_ = param_info_.on_value_changed.connect([this](float value) {
        // Update text display if not currently editing
        if (!this->is_editing_) {
            this->updateTextDisplay();
        }
    });
}

void ParamValueTextBox::updateTextDisplay() {
    std::string formatted = param_info_.valueToText(param_info_.getValue());
    text_editor_.setText(formatted);
}

void ParamValueTextBox::resized() { text_editor_.setBounds(0, 0, width(), height()); }
}  // namespace applause
