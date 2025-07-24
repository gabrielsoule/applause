#pragma once

#include "ui/Editor.h"
#include "ui/components/GenericParameterUI.h"
#include "ui/components/ParamKnob.h"
#include "extensions/ParamsExtension.h"
#include <memory>
#include <functional>

class ExampleManualPluginEntryEditor : public applause::Editor
{
public:
    explicit ExampleManualPluginEntryEditor(applause::ParamsExtension* params);
    ~ExampleManualPluginEntryEditor() override = default;
    
    void draw(visage::Canvas& canvas) override;
    void resized() override;
    
private:
    std::unique_ptr<applause::GenericParameterUI> parameter_ui_;
    std::unique_ptr<applause::ParamKnob> test_param_knob_;
    applause::ParamsExtension* params_;
};
