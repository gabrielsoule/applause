#pragma once

#include <applause/ui/ApplauseUI.h>
#include <functional>
#include <memory>

#include <applause/ui/IEditor.h>
#include <applause/extensions/ParamsExtension.h>
#include <applause/util/ParamMessageQueue.h>

namespace applause {
class TooltipDisplay;

#ifndef NDEBUG
namespace inspector { class InspectorWindow; }
#endif

/**
 * @class ApplauseEditor
 * @brief The default Applause framework editor implementation using Visage.
 *
 * This class provides a fast, "plug and play" editor implementation
 * that integrates seamlessly with the Applause parameter extension.
 *
 * For custom GUI frameworks, implement IEditor directly instead of using this
 * class.
 */
class ApplauseEditor : public IEditor,
                       public applause::ApplicationWindow,
                       public applause::EventTimer {
public:
    /**
     * @brief Construct an ApplauseEditor with parameter extension support.
     */
    explicit ApplauseEditor(ParamsExtension* params = nullptr);

    /**
     * @brief Destructor - disconnects from ParamsExtension.
     */
    ~ApplauseEditor() override;

    // IEditor interface implementation
    ParamMessageQueue* getMessageQueue() override;
    ParamsExtension* getParamsExtension();
    void show(void* parent_window) override;
    void close() override;
    uint32_t width() const override;
    uint32_t height() const override;
    void setWindowDimensions(uint32_t width, uint32_t height) override;
    void setFixedAspectRatio(bool fixed) override;
    bool isFixedAspectRatio() const override;
    float getAspectRatio() const override;
    void draw(applause::Canvas& canvas) override;
    void* getNativeHandle() override;

#ifdef __linux__
    int getPosixFd() override;
    void processPosixFdEvents() override;
#endif

    void timerCallback() override;

    TooltipDisplay& tooltipDisplay() { return *tooltip_display_; }

#ifndef NDEBUG
    // Debug-build only: the pop-out inspector window. Always non-null;
    // hidden until the user toggles it open (Cmd+I or via the Inspector
    // button in ExampleShowcase).
    inspector::InspectorWindow* inspector() { return inspector_window_.get(); }
#else
    void* inspector() { return nullptr; }
#endif

private:
    ParamMessageQueue message_queue_;  // Owned by the editor
    ParamsExtension* params_ = nullptr;
    std::unique_ptr<TooltipDisplay> tooltip_display_;
#ifndef NDEBUG
    std::unique_ptr<inspector::InspectorWindow> inspector_window_;
#endif
};
}  // namespace applause
