#pragma once

#include "ui/ApplauseEditor.h"
#include "ui/components/GenericParameterUI.h"
#include "ui/components/ParamKnob.h"
#include "extensions/ParamsExtension.h"
#include <memory>

class ExampleManualPluginEntryEditor : public applause::ApplauseEditor
{
public:
    explicit ExampleManualPluginEntryEditor(applause::ParamsExtension* params);
    ~ExampleManualPluginEntryEditor() override = default;

    void draw(visage::Canvas& canvas) override;
    void resized() override;

private:
    std::unique_ptr<applause::GenericParameterUI> parameter_ui_;
    std::unique_ptr<applause::ParamKnob> test_param_knob_;
};
