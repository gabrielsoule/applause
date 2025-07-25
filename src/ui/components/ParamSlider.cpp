#include "ParamSlider.h"
#include <visage_graphics/font.h>
#include <visage_graphics/canvas.h>
#include <visage_graphics/theme.h>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <extensions/ParamsExtension.h>

#include "embedded/fonts.h"
#include "util/DebugHelpers.h"

namespace applause {

// Define a transparent background color for text editors
VISAGE_THEME_COLOR(TransparentBackground, 0x00000000);

// Define inverted terminal-style colors for text editor
VISAGE_THEME_COLOR(TerminalText, 0xFF000000);           // Black text
VISAGE_THEME_COLOR(TerminalSelection, 0xFF000000);      // Black selection background
VISAGE_THEME_COLOR(TerminalCaret, 0xFF000000);          // Black caret
VISAGE_THEME_COLOR(TerminalDefaultText, 0xFF666666);    // Dark gray placeholder


ParamSlider::ParamSlider(ParamInfo& paramInfo) : param_info_(paramInfo)
{
    slider_ = std::make_unique<Slider>();
    addChild(slider_.get());

    // Initialize standard TextEditor
    auto text_editor = std::make_unique<visage::TextEditor>("param_value");
    text_editor->setMultiLine(false);  // Single line
    text_editor->setJustification(visage::Font::kCenter);  // Center text
    text_editor->setFont(visage::Font(14, visage::fonts::DroidSansMono_ttf));

    // Make the background transparent by using our transparent color ID
    text_editor->setBackgroundColorId(TransparentBackground);

    // Store the raw pointer before moving
    text_editor_ = text_editor.get();
    
    addChild(text_editor.release());

    // Set initial value (normalized to 0-1 range)
    const float currentValue = param_info_.getValue();
    const float normalizedValue = (currentValue - param_info_.minValue) / (param_info_.maxValue - param_info_.minValue);
    slider_->setValue(normalizedValue);
    LOG_DBG("ParamSlider value: {} min: {} max: {}", param_info_.getValue(), param_info_.minValue, param_info_.maxValue);
    
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
            
            // Update slider to match
            float normalizedValue = (parsed_value.value() - param_info_.minValue) / 
                                   (param_info_.maxValue - param_info_.minValue);
            slider_->setValue(normalizedValue);
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
        
        // Update slider to match
        float normalizedValue = (original_value_ - param_info_.minValue) / 
                               (param_info_.maxValue - param_info_.minValue);
        slider_->setValue(normalizedValue);
        
        // Update display to show original value
        updateTextDisplay();
        is_editing_ = false;
        param_info_.endGesture();
    };

    // Add our callbacks to hook the slider's state into the parameter's state
    slider_->on_value_changed.add([this](float value)
    {
        const float paramValue = this->param_info_.minValue + value * (this->param_info_.maxValue - this->param_info_.
            minValue);
        this->param_info_.setValueNotifyingHost(paramValue);
        
        // Update text display if not currently editing
        if (!this->is_editing_)
        {
            this->updateTextDisplay();
        }
    });

    // Connect gesture events
    slider_->on_drag_started.add([this]()
    {
        this->param_info_.beginGesture();
    });

    slider_->on_drag_ended.add([this]()
    {
        this->param_info_.endGesture();
    });

    // Connect to parameter changes from the host
    param_connection_ = param_info_.on_value_changed.connect([this](float value)
    {
        // Update slider when parameter changes externally
        const float normalizedValue = (value - this->param_info_.minValue) /
            (this->param_info_.maxValue - this->param_info_.minValue);
        this->slider_->setValue(normalizedValue);
        
        // Update text display if not currently editing
        if (!this->is_editing_)
        {
            this->updateTextDisplay();
        }
    });
}

void ParamSlider::updateTextDisplay()
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


void ParamSlider::draw(visage::Canvas& canvas)
{

}

void ParamSlider::resized()
{
    // Position the slider on the left side, leaving space for text on the right
    float sliderWidth = width() - kLabelWidth;
    if (slider_)
    {
        slider_->setBounds(0, 0, sliderWidth, height());
    }
    
    if (text_editor_)
    {
        float textX = width() - kLabelWidth + kLabelPadding;
        float textWidth = kLabelWidth - kLabelPadding;
        text_editor_->setBounds(textX, 0, textWidth, height());
    }
}

} // namespace applause
