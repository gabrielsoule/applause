#pragma once

#include "thirdparty/readerwriterqueue.h"

namespace applause {
/**
 * @class ParamMessageQueue
 * @brief Simple utility to facilitate communication between UI and audio
 * threads through a message-passing mechanism.
 *
 * The MessageQueue class provides two distinct message queues for bidirectional
 * communication between the UI thread and the audio thread. Messages consist of
 * a type, a parameter ID, and a value.
 */
class ParamMessageQueue {
   public:
    enum MessageType { PARAM_VALUE, BEGIN_GESTURE, END_GESTURE };

    struct Message {
        MessageType type;
        uint32_t paramId;
        float value;
    };

    applause::ReaderWriterQueue<Message>& toAudio() { return ui2audio_; }
    applause::ReaderWriterQueue<Message>& toUi() { return audio2ui_; }

   private:
    applause::ReaderWriterQueue<Message> ui2audio_;
    applause::ReaderWriterQueue<Message> audio2ui_;
};
}  // namespace applause
