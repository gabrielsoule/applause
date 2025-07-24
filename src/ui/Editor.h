#pragma once
#include "util/ParamMessageQueue.h"
#include "visage_app/application_window.h"
#include "extensions/ParamsExtension.h"
#include <functional>

namespace applause
{
    /**
     * @class Editor
     * @brief A simple wrapper around visage::ApplicationWindow that handles some useful plugin-related utilities
     * for you, such as thread-safe parameter updates to and from the host.
     * 
     * By default, the Editor provides built-in support for ParamsExtension.
     * In the future, this will be properly modularized so that the Editor and its associated
     * parameter widgets can be used with any compliant parameter extension.
     */
    class Editor : public visage::ApplicationWindow, public visage::EventTimer
    {
    public:
        explicit Editor(ParamsExtension* params = nullptr);
        void timerCallback() override;
        
        // Get the message queue for thread-safe parameter communication
        ParamMessageQueue* getMessageQueue() { return &message_queue_; }
        
    private:
        ParamMessageQueue message_queue_;
        ParamsExtension* params_ = nullptr;
    };
}
