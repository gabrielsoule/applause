#pragma once

#include "applause/ui/ApplauseEditor.h"
#include "applause/ui/components/GenericParameterUI.h"
#include "applause/ui/components/ParamKnob.h"
#include "applause/extensions/ParamsExtension.h"
#include <memory>

class ExampleManualPluginEntryEditor : public applause::ApplauseEditor
{
public:
    explicit ExampleManualPluginEntryEditor(applause::ParamsExtension* params);
    ~ExampleManualPluginEntryEditor() override = default;

    void resized() override;

private:
    std::unique_ptr<applause::GenericParameterUI> parameter_ui_;
    std::unique_ptr<applause::ParamKnob> test_param_knob_;
};
