#include "ParamValueTextBox.h"
#include <visage_graphics/font.h>
#include <sstream>
#include <iomanip>
#include <cmath>

#include "embedded/fonts.h"
#include "util/DebugHelpers.h"

namespace applause {

ParamValueTextBox::ParamValueTextBox(ParamInfo& paramInfo) : param_info_(paramInfo)
{
    // Initialize and configure custom palette for terminal-style theming
    custom_palette_.initWithDefaults();
    custom_palette_.setColor(visage::TextEditor::TextEditorBackground, 0x00000000);  // Fully transparent
    custom_palette_.setColor(visage::TextEditor::TextEditorText, 0xFFFFFFFF);       // White text
    custom_palette_.setColor(visage::TextEditor::TextEditorCaret, 0xFFFFFFFF);      // White caret
    // Don't override selection color - use default light gray from initWithDefaults()
    custom_palette_.setColor(visage::TextEditor::TextEditorDefaultText, 0xFF999999); // Light gray for placeholder
    custom_palette_.setColor(visage::TextEditor::TextEditorBorder, 0x00000000);     // No border
    
    // Apply the custom palette to this component and its children
    setPalette(&custom_palette_);
    
    // Initialize standard TextEditor
    auto text_editor = std::make_unique<visage::TextEditor>("param_value");
    text_editor->setMultiLine(false);  // Single line
    text_editor->setJustification(visage::Font::kCenter);  // Center text
    text_editor->setFont(visage::Font(14, visage::fonts::DroidSansMono_ttf));

    // Store the raw pointer before moving
    text_editor_ = text_editor.get();
    
    addChild(text_editor.release());

    // Display initial value in text editor
    updateTextDisplay();

    // Lambda to commit the current text value (shared by Enter and focus lost)
    auto commitValue = [this]()
    {
        if (!is_editing_)
            return;
            
        // Try to parse the entered text
        auto parsed_value = param_info_.parseText(text_editor_->text().toUtf8());
        
        if (parsed_value.has_value())
        {
            // Valid value entered - apply it
            param_info_.setValueNotifyingHost(parsed_value.value());
        }
        
        // Update display to show formatted value
        updateTextDisplay();
        is_editing_ = false;
        param_info_.endGesture();
    };

    // Set up TextEditor callbacks
    text_editor_->onTextChange() += [this]()
    {
        if (!is_editing_)
        {
            // Save original value when editing starts
            original_value_ = param_info_.getValue();
            is_editing_ = true;
            param_info_.beginGesture();
        }
    };

    text_editor_->onEnterKey() += commitValue;
    
    // Use built-in onFocusChange() callback to commit when focus is lost
    text_editor_->onFocusChange() += [this, commitValue](bool is_focused, bool was_clicked)
    {
        if (!is_focused)  // Lost focus
        {
            commitValue();
        }
    };

    text_editor_->onEscapeKey() += [this]()
    {
        // Restore original value
        param_info_.setValueNotifyingHost(original_value_);
        
        // Update display to show original value
        updateTextDisplay();
        is_editing_ = false;
        param_info_.endGesture();
    };

    // Connect to parameter changes from the host
    param_connection_ = param_info_.on_value_changed.connect([this](float value)
    {
        // Update text display if not currently editing
        if (!this->is_editing_)
        {
            this->updateTextDisplay();
        }
        
        LOG_DBG("ParamValueTextBox: Parameter value changed to {}", value);
    });
}

void ParamValueTextBox::updateTextDisplay()
{
    float currentValue = param_info_.getValue();
    
    // Format the value as a string
    std::ostringstream valueStream;
    if (param_info_.stepped)
    {
        valueStream << static_cast<int>(currentValue);
    }
    else
    {
        // Adaptive precision to fit within character limit
        const int maxChars = 5;  // Maximum characters to display
        const int maxDecimals = 2;  // Never show more than 2 decimal places
        
        // Calculate how many characters we need for the integer part and sign
        float absValue = std::abs(currentValue);
        int integerDigits = (absValue >= 1.0f) ? static_cast<int>(std::log10(absValue)) + 1 : 1;
        int signChars = (currentValue < 0) ? 1 : 0;
        
        // Check if we can fit any decimals
        int usedChars = integerDigits + signChars;
        if (usedChars >= maxChars) {
            // Number is too large for any decimals
            valueStream << std::fixed << std::setprecision(0) << currentValue;
        }
        else {
            // Calculate available space for decimal point + decimal digits
            int availableForDecimal = maxChars - usedChars;
            
            // We need at least 2 characters for decimal point + 1 digit
            if (availableForDecimal >= 2) {
                // Use up to maxDecimals, but no more than what fits
                int decimalsToShow = std::min(maxDecimals, availableForDecimal - 1);
                valueStream << std::fixed << std::setprecision(decimalsToShow) << currentValue;
            }
            else {
                // No room for decimals
                valueStream << std::fixed << std::setprecision(0) << currentValue;
            }
        }
    }
    
    text_editor_->setText(valueStream.str());
}

void ParamValueTextBox::resized()
{
    if (text_editor_)
    {
        text_editor_->setBounds(0, 0, width(), height());
    }
}

} // namespace applause