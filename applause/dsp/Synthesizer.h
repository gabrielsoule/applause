#pragma once
#include <array>

namespace applause {
class SynthesizerVoice {
   public:
    virtual ~SynthesizerVoice() = default;

    bool active = false;
    int midi_note_number_ = 0;
    int play_order_ = 0;

    enum class State {
        Idle,     // the voice is idle and ready to play a note
        KeyDown,  // the note is actively playing a note, and the corresponding
                  // key is down
        Sustained,  // the voice is no longer playing a note, but the sustain
                    // pedal is still down
        Released,   // the voice is still playing a note, but the corresponding
                    // key is released (e.g. release phase)
        Stolen,  // the voice is currently being stolen. The voice should fade
                 // out as quickly as possible here.
    };

    State state_ = State::Idle;
};

/**
A simple synthesizer that does everything you need and nothing that you don't.
To use, extend SynthesizerVoice and add your own DSP code. Supports both
standard MIDI and MPE.

This class is designed for performance and does not invoke heap allocations.
Many parameters, such as the maximum number of voices, are templated at compile
time.
*/
template <int num_voices>
class Synthesizer {
   public:
    [[nodiscard]] int getNumVoices() const noexcept { return num_voices; }

   private:
    std::array<SynthesizerVoice, num_voices> voices_;
    int notes_played_ = 0;  // count the number of notes; used for finding the
                            // oldest voice during voice stealing
};
}  // namespace applause
