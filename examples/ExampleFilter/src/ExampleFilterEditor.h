#pragma once

#include "applause/ui/ApplauseEditor.h"
#include "applause/ui/components/ParamKnob.h"
#include "applause/extensions/ParamsExtension.h"
#include <memory>

class ExampleFilterEditor : public applause::ApplauseEditor
{
public:
    explicit ExampleFilterEditor(applause::ParamsExtension* params);
    ~ExampleFilterEditor() override = default;
    
    void resized() override;
    
private:
    std::unique_ptr<applause::ParamKnob> cutoff_knob_;
    std::unique_ptr<applause::ParamKnob> resonance_knob_;
    std::unique_ptr<applause::ParamKnob> mode_knob_;
};