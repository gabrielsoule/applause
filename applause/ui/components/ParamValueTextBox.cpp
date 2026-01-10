#include "ParamValueTextBox.h"

#include <embedded/applause_fonts.h>
#include <visage_graphics/font.h>

#include <cmath>
#include <iomanip>

#include "applause/util/DebugHelpers.h"

namespace applause {
VISAGE_THEME_IMPLEMENT_COLOR(ParamValueTextBox, ApplauseTextEditorBackground, 0x00000000);  // Transparent
VISAGE_THEME_IMPLEMENT_COLOR(ParamValueTextBox, ApplauseTextEditorBorder, 0x00000000);  // No border
VISAGE_THEME_IMPLEMENT_COLOR(ParamValueTextBox, ApplauseTextEditorText, 0xFFFFFFFF);  // White text
VISAGE_THEME_IMPLEMENT_COLOR(ParamValueTextBox, ApplauseTextEditorDefaultText, 0xFF999999);  // Light gray
VISAGE_THEME_IMPLEMENT_COLOR(ParamValueTextBox, ApplauseTextEditorCaret, 0xFFFFFFFF);  // White caret
VISAGE_THEME_IMPLEMENT_COLOR(ParamValueTextBox, ApplauseTextEditorSelection, 0x22ffffff);  // Light selection
VISAGE_THEME_IMPLEMENT_VALUE(ParamValueTextBox, ApplauseTextEditorRounding, 0.0f);  // No rounding

ParamValueTextBox::ParamValueTextBox(ParamInfo& paramInfo)
    : param_info_(paramInfo) {
    text_editor_ = std::make_unique<visage::TextEditor>("param_value");
    text_editor_->setMultiLine(false);
    text_editor_->setJustification(visage::Font::kCenter);
    text_editor_->setFont(
        visage::Font(12, applause::fonts::Jost_Medium_ttf));
    text_editor_->setMargin(0, 0);
    addChild(text_editor_.get());

    updateTextDisplay();

    // Lambda to commit the current text value (shared by Enter and focus lost)
    auto commitValue = [this]() {
        if (!is_editing_) return;

        // Try to parse the entered text
        auto parsed_value =
            param_info_.textToValue(text_editor_->text().toUtf8());

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
    text_editor_->onTextChange() += [this]() {
        if (!is_editing_) {
            // Save original value when editing starts
            original_value_ = param_info_.getValue();
            is_editing_ = true;
            param_info_.beginGesture();
        }
    };

    text_editor_->onEnterKey() += commitValue;

    text_editor_->onFocusChange() +=
        [this, commitValue](bool is_focused, bool was_clicked) {
            if (!is_focused)
            {
                commitValue();
            }
        };

    text_editor_->onEscapeKey() += [this]() {
        // Restore original value
        param_info_.setValueNotifyingHost(original_value_);

        // Update display to show original value
        updateTextDisplay();
        is_editing_ = false;
        param_info_.endGesture();
    };

    // Connect to parameter changes from the host
    param_connection_ =
        param_info_.on_value_changed.connect([this](float value) {
            // Update text display if not currently editing
            if (!this->is_editing_) {
                this->updateTextDisplay();
            }
        });
}

void ParamValueTextBox::init() {
    // Call parent init
    visage::Frame::init();

    // Map our Applause theme colors to TextEditor's expected theme IDs
    // Basically, each ParamValueTextBox reads the global ApplauseXYZ color IDs,
    // makes a local palette, and tells the underlying editor (which this class
    // extends) to use that palette. This allows users to customize appearance
    // via a top-level palette, while keeping the default Visage text editor
    // theme untouched. This is a bit of a hack, but it does the trick.
    custom_palette_.initWithDefaults();
    custom_palette_.setColor(visage::TextEditor::TextEditorBackground,
                             paletteColor(ApplauseTextEditorBackground));
    custom_palette_.setColor(visage::TextEditor::TextEditorBorder,
                             paletteColor(ApplauseTextEditorBorder));
    custom_palette_.setColor(visage::TextEditor::TextEditorText,
                             paletteColor(ApplauseTextEditorText));
    custom_palette_.setColor(visage::TextEditor::TextEditorDefaultText,
                             paletteColor(ApplauseTextEditorDefaultText));
    custom_palette_.setColor(visage::TextEditor::TextEditorCaret,
                             paletteColor(ApplauseTextEditorCaret));
    custom_palette_.setColor(visage::TextEditor::TextEditorSelection,
                             paletteColor(ApplauseTextEditorSelection));

    // TODO: Also handle rounding value if needed
    // float rounding = paletteValue(ApplauseTextEditorRounding);
    // text_editor_->setBackgroundRounding(rounding);

    setPalette(&custom_palette_);
}

void ParamValueTextBox::updateTextDisplay() {
    std::string formatted = param_info_.valueToText(param_info_.getValue());
    text_editor_->setText(formatted);
}

void ParamValueTextBox::resized() {
    if (text_editor_) {
        text_editor_->setBounds(0, 0, width(), height());
    }
}
}  // namespace applause
