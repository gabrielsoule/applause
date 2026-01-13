#pragma once
#include <functional>

#include "IEditor.h"
#include "applause/extensions/ParamsExtension.h"
#include "applause/util/ParamMessageQueue.h"
#include "visage_app/application_window.h"
#include "visage_graphics/palette.h"

namespace applause {
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
                       public visage::ApplicationWindow,
                       public visage::EventTimer {
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
    void draw(visage::Canvas& canvas) override;
    void* getNativeHandle() override;

#ifdef __linux__
    int getPosixFd() override;
    void processPosixFdEvents() override;
#endif

    void timerCallback() override;

private:
    ParamMessageQueue message_queue_;  // Owned by the editor
    ParamsExtension* params_ = nullptr;
};
}  // namespace applause
