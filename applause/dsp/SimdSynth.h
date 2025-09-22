#pragma once

namespace applause {

struct VoiceInfo {
    enum KeyState { Idle, Pressed, Released };
};

class AggregateVoice {};

class SIMDSynth {};
}  // namespace applause