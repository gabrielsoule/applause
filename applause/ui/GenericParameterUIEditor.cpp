#include "GenericParameterUIEditor.h"
#include "applause/util/DebugHelpers.h"

namespace applause {

GenericParameterUIEditor::GenericParameterUIEditor(ParamsExtension* params)
    : ApplauseEditor(params)
{
    // Create the GenericParameterUI component
    parameter_ui_ = std::make_unique<GenericParameterUI>();
    
    // Add all non-internal parameters to the UI
    if (getParamsExtension())
    {
        for (auto& param : getParamsExtension()->getAllParameters())
        {
            if (!param.internal)
            {
                parameter_ui_->addParameter(param);
            }
        }
    }
    
    // Add the parameter UI as a child component
    addChild(parameter_ui_.get());
}

void GenericParameterUIEditor::resized()
{
    // Make the GenericParameterUI fill the entire window with a small padding
    static constexpr int kPadding = 20;
    parameter_ui_->setBounds(kPadding, kPadding, 
                           width() - 2 * kPadding, 
                           height() - 2 * kPadding);
}

} // namespace applause
