#pragma once

#include <memory>

#include "ApplauseEditor.h"
#include "applause/extensions/ParamsExtension.h"
#include "components/GenericParameterUI.h"

namespace applause {

/**
 * @class GenericParameterUIEditor
 * @brief A plug-and-play editor that automatically displays all parameters
 * using GenericParameterUI.
 *
 * This editor provides a simple solution for displaying all plugin parameters
 * without requiring custom UI implementation. It automatically populates a
 * GenericParameterUI with all non-internal parameters from the provided
 * ParamsExtension.
 *
 * The editor size is configured through the GUIExtension factory, and the
 * GenericParameterUI automatically fills the window with appropriate padding.
 */
class GenericParameterUIEditor : public ApplauseEditor {
public:
    /**
     * @brief Construct a GenericParameterUIEditor.
     * @param params The parameter extension containing all plugin parameters
     */
    explicit GenericParameterUIEditor(ParamsExtension* params);

    /**
     * @brief Destructor.
     */
    ~GenericParameterUIEditor() override = default;

    /**
     * @brief Called when the editor is resized to layout components.
     */
    void resized() override;

private:
    std::unique_ptr<GenericParameterUI> parameter_ui_;
};

}  // namespace applause