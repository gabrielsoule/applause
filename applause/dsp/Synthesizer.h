#pragma once
#include <applause/util/SampleType.h>
#include <applause/core/ProcessInfo.h>

#include <algorithm>
#include <array>
#include <type_traits>

#include <clap/events.h>

#include "BufferView.h"

namespace applause {

/**
 *
 * The voice is templated with both the sample type (float/double) and the
 * maximum number of channels supported by the DSP graph. In practice, the
 * number of active channels can be fewer. For example, one may template with
 * respect to 8 maximum channels, for surround sound compatibility, but work
 * with two-channel stereo sound most of the time.
 *
 * @tparam T The sample type of the voice (float or double).
 * @tparam MaxChannels The maximum number of channels supported by the DSP
 * system
 *
 * All voices live in a pool where they can be reused indefinitely.
 */

template <Scalar T, size_t MaxChannels>
class SynthesizerVoice {
public:
    virtual ~SynthesizerVoice() = default;

    virtual void process(BufferView<T, MaxChannels> buffer,
                         int start_sample, int num_samples) = 0;
    /**
     * Terminates the voice immediately, releasing it back into the pool
     * for reuse. The voice should call its own terminateVoice() function
     * once its corresponding key has been released and it is no longer
     * producing any audio.
     *
     * Make sure to fade out your voice's audio before terminating the voice!
     */

    virtual void noteOn() {}

    /**
     * Called when this voice's MIDI note is released.
     *
     * This function can also be called when the voice is to be stolen,
     *
     *
     * @param terminate_now Set to true when the voice is set to be stolen; if true,
     * the voice must immediately call terminateVoice() to release itself back
     * into the voice pool, and perform any other necessary cleanup.
     */
    virtual void noteOff(bool terminate_now) {
        if (terminate_now) {
            terminateVoice();
        }
    }

    /**
     * Mark this voice as finished with the current note, releasing it back into
     * the pool for reuse. Voices should call this function when (a) the corresponding key is
     * released and (b) the voice is no longer producing any audio.
     */
    void terminateVoice() {
        active_ = false;
        state_ = State::Idle;
    }

    double getSampleRate() const noexcept { return sample_rate_; }
    void setSampleRate(double sample_rate) noexcept { sample_rate_ = sample_rate; }

    bool active_ = false;
    int midi_note_number_ = 0;
    int velocity_ = 0;
    int play_order_ = 0;

    enum class State {

        Idle,     // the voice is idle and ready to play a note
        KeyDown,  // the note is actively playing a note, and the corresponding
                  // key is down
        Sustained,  // the voice is no longer playing a note, but the sustain
                    // pedal is still down
        Released,  // the voice is still playing a note, but the correspondingey
                   // is released (e.g. release phase)
    };

    State state_ = State::Idle;

protected:
    double sample_rate_ = 44100.0;
};

/**
A simple synthesizer that does everything you need and nothing that you don't.
To use, extend SynthesizerVoice and add your own DSP code. Supports both
standard MIDI and MPE.

This class is designed for performance and does not do heap allocations.
Many parameters, such as the maximum number of voices and audio channels,
are templated at compile time.

@tparam T The sample type (float or double)
@tparam MaxChannels Maximum number of audio channels
@tparam NumVoices Maximum number of polyphonic voices
@tparam VoiceType The concrete voice class (must derive from SynthesizerVoice)
*/
template <Scalar T, size_t MaxChannels = 8, size_t NumVoices = 16, typename VoiceType = SynthesizerVoice<T, MaxChannels>>
class Synthesizer {
    static_assert(std::is_base_of_v<SynthesizerVoice<T, MaxChannels>, VoiceType>,
                  "VoiceType must derive from SynthesizerVoice<T, MaxChannels>");

public:
    [[nodiscard]] int getNumVoices() const noexcept { return NumVoices; }

    void activate(ProcessInfo info);
    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote, float velocity);
    VoiceType& findFreeVoice();
    VoiceType& stealVoice();
    void process(BufferView<T, MaxChannels> buffer, const clap_input_events_t* events);

private:
    std::array<VoiceType, NumVoices> voices_;
    int notes_played_ = 0;  // count the number of notes; used for finding the
                            // oldest voice during voice stealing
};

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
void Synthesizer<T, MaxChannels, NumVoices, VoiceType>::activate(ProcessInfo info) {
    for (auto& voice : voices_) {
        voice.setSampleRate(info.sample_rate);
    }
}

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
VoiceType& Synthesizer<T, MaxChannels, NumVoices, VoiceType>::findFreeVoice() {
    for (auto& voice : voices_) {
        if (!voice.active_ || voice.state_ == SynthesizerVoice<T, MaxChannels>::State::Idle) {
            return voice;
        }
    }

    return stealVoice();
}

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
VoiceType& Synthesizer<T, MaxChannels, NumVoices, VoiceType>::stealVoice() {
    VoiceType* oldest = &voices_[0];
    for (auto& voice : voices_) {
        if (voice.play_order_ < oldest->play_order_) {
            oldest = &voice;
        }
    }

    oldest->noteOff(true);
    return *oldest;
}

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
void Synthesizer<T, MaxChannels, NumVoices, VoiceType>::noteOn(int midiNote, float velocity) {
    VoiceType& voice = findFreeVoice();

    voice.midi_note_number_ = midiNote;
    voice.velocity_ = static_cast<int>(velocity * 127.0f);
    voice.play_order_ = notes_played_++;
    voice.state_ = SynthesizerVoice<T, MaxChannels>::State::KeyDown;
    voice.active_ = true;

    voice.noteOn();
}

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
void Synthesizer<T, MaxChannels, NumVoices, VoiceType>::noteOff(int midiNote, float velocity) {
    for (auto& voice : voices_) {
        if (voice.active_ && voice.midi_note_number_ == midiNote &&
            voice.state_ == SynthesizerVoice<T, MaxChannels>::State::KeyDown) {
            voice.noteOff(false);
            voice.state_ = SynthesizerVoice<T, MaxChannels>::State::Released;
        }
    }
}

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
void Synthesizer<T, MaxChannels, NumVoices, VoiceType>::process(BufferView<T, MaxChannels> buffer,
                                                                  const clap_input_events_t* events) {
    buffer.clear();

    const uint32_t total_frames = buffer.numFrames();
    uint32_t current_sample = 0;

    if (events) {
        const uint32_t event_count = events->size(events);

        for (uint32_t i = 0; i < event_count; ++i) {
            const clap_event_header_t* header = events->get(events, i);
            if (!header || header->space_id != CLAP_CORE_EVENT_SPACE_ID) {
                continue;
            }

            const uint32_t event_time = std::min(header->time, total_frames);

            // Render chunk before this event
            if (event_time > current_sample) {
                const int num_samples = event_time - current_sample;
                for (auto& voice : voices_) {
                    if (voice.active_) {
                        voice.process(buffer, current_sample, num_samples);
                    }
                }
            }

            // Handle note events
            if (header->type == CLAP_EVENT_NOTE_ON) {
                const auto* note_event = reinterpret_cast<const clap_event_note_t*>(header);
                noteOn(note_event->key, note_event->velocity);
            } else if (header->type == CLAP_EVENT_NOTE_OFF) {
                const auto* note_event = reinterpret_cast<const clap_event_note_t*>(header);
                noteOff(note_event->key, note_event->velocity);
            }

            current_sample = event_time;
        }
    }

    // Render remaining samples after last event
    if (current_sample < total_frames) {
        const int num_samples = total_frames - current_sample;
        for (auto& voice : voices_) {
            if (voice.active_) {
                voice.process(buffer, current_sample, num_samples);
            }
        }
    }
}

}  // namespace applause
