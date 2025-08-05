#pragma once

#include <visage_ui/frame.h>
#include <visage_widgets/text_editor.h>
#include <visage_graphics/palette.h>
#include "applause/util/thirdparty/rocket.hpp"
#include "applause/extensions/ParamsExtension.h"

namespace applause {

/**
 * A text box component that displays and allows editing of a parameter value.
 * Features adaptive precision formatting and validates input using ParamInfo::parseText.
 */
class ParamValueTextBox : public visage::Frame
{
public:
    // Theme colors for customization
    VISAGE_THEME_DEFINE_COLOR(ApplauseTextEditorBackground);
    VISAGE_THEME_DEFINE_COLOR(ApplauseTextEditorBorder);
    VISAGE_THEME_DEFINE_COLOR(ApplauseTextEditorText);
    VISAGE_THEME_DEFINE_COLOR(ApplauseTextEditorDefaultText);
    VISAGE_THEME_DEFINE_COLOR(ApplauseTextEditorCaret);
    VISAGE_THEME_DEFINE_COLOR(ApplauseTextEditorSelection);
    VISAGE_THEME_DEFINE_VALUE(ApplauseTextEditorRounding);

    explicit ParamValueTextBox(ParamInfo& paramInfo);
    
    void resized() override;
    void init() override;
    
    /** Force update the displayed text to match the current parameter value */
    void updateTextDisplay();
    
private:
    ParamInfo& param_info_;
    std::unique_ptr<visage::TextEditor> text_editor_ = nullptr;  // Raw pointer, owned by parent
    rocket::scoped_connection param_connection_;
    
    // Custom palette for terminal-style theming
    visage::Palette custom_palette_;
    
    // Edit state tracking
    float original_value_ = 0.0f;
    bool is_editing_ = false;
};

} // namespace applause