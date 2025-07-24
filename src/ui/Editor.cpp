#include "Editor.h"
#include "util/DebugHelpers.h"

applause::Editor::Editor(ParamsExtension* params) : params_(params)
{
    if (params) startTimer(30);
    else LOG_WARN("Editor instantiated without a pointer to an instance of applause::ParamsExtension");
}

void applause::Editor::timerCallback()
{
    ParamMessageQueue::Message msg{};
    while (message_queue_.toUi().try_dequeue(msg))
    {
        if (msg.type == ParamMessageQueue::MessageType::PARAM_VALUE)
        {
            auto& param = params_->getInfo(msg.paramId);
            param.on_value_changed(msg.value);
        }
        else
        {
            ASSERT_FALSE("Editor received unexpected message type");
        }
    }
}
