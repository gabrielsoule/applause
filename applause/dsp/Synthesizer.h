#pragma once
#include <applause/util/SampleType.h>

#include <array>

#include "BufferView.h"

// UNFINISHED DO NOT USE
namespace applause {

/**
 *
 * The voice is templated with both the sample type (float/double) and the
 * maximum number of channels supported by the DSP graph. In practice, the
 * number of active channels can be fewer. For example, one may template with
 * respect to 8 maximum channels, for surround sound compatibility, but work
 * with two-channel stereo sound most of the time.
 *
 * @tparam Scalar The sample type of the voice (float or double).
 * @tparam max_channels The maximum number of channels supported by the DSP
 * system
 */
template <ScalarType Scalar, size_t max_channels>
class SynthesizerVoice {
   public:
    virtual ~SynthesizerVoice() = default;

    virtual void process(BufferView<Scalar, max_channels> buffer,
                         int start_sample, int num_samples) = 0;

    bool active = false;
    int midi_note_number_ = 0;
    int play_order_ = 0;

    enum class State {

        Idle,     // the voice is idle and ready to play a note
        KeyDown,  // the note is actively playing a note, and the corresponding
                  // key is down
        Sustained,  // the voice is no longer playing a note, but the sustain
                    // pedal is still down
        Released,  // the voice is still playing a note, but the correspondingey
                   // is released (e.g. release phase)
        Stolen,    // the voice is currently being stolen. The voice should fade
                   // out as quickly as possible here.
    };

    State state_ = State::Idle;
};

/**
A simple synthesizer that does everything you need and nothing that you don't.
To use, extend SynthesizerVoice and add your own DSP code. Supports both
standard MIDI and MPE.

This class is designed for performance and does not do heap allocations.
Many parameters, such as the maximum number of voices and audio channels,
are templated at compile time.
*/
template <ScalarType Scalar, int num_voices>
class Synthesizer {
   public:
    [[nodiscard]] int getNumVoices() const noexcept { return num_voices; }

   private:
    constexpr static int max_channels = 8;
    std::array<SynthesizerVoice<Scalar, max_channels>, num_voices> voices_;
    int notes_played_ = 0;  // count the number of notes; used for finding the
                            // oldest voice during voice stealing
};
}  // namespace applause
