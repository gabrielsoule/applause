#pragma once
#include <applause/util/SampleType.h>
#include <applause/core/ProcessInfo.h>

#include <algorithm>
#include <array>
#include <type_traits>

#include <clap/events.h>

#include "BufferView.h"
#include "Note.h"

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

    virtual void noteOn() {
    }

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
     * Called when a note expression changes for this voice.
     *
     * This callback allows voices to react immediately to expression changes
     * (like pitch bend, brightness, pressure) without having to check every sample.
     *
     * Voices that cache computed values (like phase increment from frequency)
     * should override this to recalculate when relevant expressions change.
     *      * @param expression_id The expression that changed (Note::Expression::Tuning, etc.)
     * @param value The new value for this expression
     *
     * Example:
     * @code
     * void onExpressionChange(Note::Expression expression_id, double value) override {
     *     if (expression_id == Note::Expression::Tuning) {
     *         // Recalculate frequency with new tuning
     *         phase_increment_ = calculatePhaseIncrement(note_.getFrequency());
     *     }
     * }
     * @endcode
     */
    virtual void onExpressionChange(Note::Expression expression_id, double value) {
        // Default: do nothing (voice implementation may recalculate per-sample instead)
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

    void setSampleRate(double sample_rate) noexcept {
        sample_rate_ = sample_rate;
    }

    /**
     * The note data for this voice, including all CLAP note expressions.
     * Voice implementations can access note_.key, note_.getFrequency(),
     * note_.expression, note_.brightness, note_.tuning, etc.
     */
    Note note_;

    // Voice management state (separate from note data)
    bool active_ = false;
    int play_order_ = 0;

    enum class State {
        Idle, // the voice is idle and ready to play a note
        KeyDown, // the note is actively playing a note, and the corresponding key is down
        Sustained, // the voice is no longer playing a note, but the sustain pedal is still down
        Released, // the voice is still playing a note, but the corresponding key is released (e.g. release phase)
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
are templated at compile time. Sorry!

@tparam T The sample type (float or double)
@tparam MaxChannels Maximum number of audio channels
@tparam NumVoices Maximum number of polyphonic voices
@tparam VoiceType The concrete voice class (must derive from SynthesizerVoice)
*/
template <Scalar T, size_t MaxChannels = 8, size_t NumVoices = 16, typename
                    VoiceType = SynthesizerVoice<
                        T, MaxChannels>>
class Synthesizer {
    static_assert(
        std::is_base_of_v<SynthesizerVoice<T, MaxChannels>, VoiceType>,
        "VoiceType must derive from SynthesizerVoice<T, MaxChannels>");

public:
    [[nodiscard]] int getNumVoices() const noexcept { return NumVoices; }

    void activate(ProcessInfo info);
    void noteOn(const clap_event_note_t* event);
    void noteOff(const clap_event_note_t* event);
    void noteChoke(const clap_event_note_t* event);
    VoiceType& findFreeVoice();
    VoiceType& stealVoice();
    void process(BufferView<T, MaxChannels> buffer,
                 const clap_input_events_t* events);

private:
    std::array<VoiceType, NumVoices> voices_;
    int notes_played_ = 0; // count the number of notes; used for finding the
    // oldest voice during voice stealing
};

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
void Synthesizer<T, MaxChannels, NumVoices, VoiceType>::activate(
    ProcessInfo info) {
    for (auto& voice : voices_) {
        voice.setSampleRate(info.sample_rate);
    }
}

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
VoiceType& Synthesizer<T, MaxChannels, NumVoices, VoiceType>::findFreeVoice() {
    for (auto& voice : voices_) {
        if (!voice.active_ || voice.state_ == SynthesizerVoice<
                T, MaxChannels>::State::Idle) {
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
void Synthesizer<T, MaxChannels, NumVoices, VoiceType>::noteOn(const clap_event_note_t* event) {
    VoiceType& voice = findFreeVoice();

    // Use Note struct to store all note data with full precision
    voice.note_ = Note::fromNoteOn(event);
    voice.play_order_ = notes_played_++;
    voice.state_ = SynthesizerVoice<T, MaxChannels>::State::KeyDown;
    voice.active_ = true;

    voice.noteOn();
}

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
void Synthesizer<T, MaxChannels, NumVoices, VoiceType>::noteOff(const clap_event_note_t* event) {
    for (auto& voice : voices_) {
        if (voice.active_ && voice.state_ == SynthesizerVoice<T, MaxChannels>::State::KeyDown) {
            // Use CLAP wildcard matching: (port, channel, key, note_id)
            if (voice.note_.matches(event->key, event->note_id, event->port_index, event->channel)) {
                voice.note_.setNoteOff(event);
                voice.noteOff(false);
                voice.state_ = SynthesizerVoice<T, MaxChannels>::State::Released;
                // If specific note_id provided, only release that one voice
                if (event->note_id != -1) break;
            }
        }
    }
}

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
void Synthesizer<T, MaxChannels, NumVoices, VoiceType>::noteChoke(const clap_event_note_t* event) {
    for (auto& voice : voices_) {
        if (voice.active_) {
            // Use CLAP wildcard matching: (port, channel, key, note_id)
            if (voice.note_.matches(event->key, event->note_id, event->port_index, event->channel)) {
                voice.noteOff(true); // Terminate immediately
                // If specific note_id provided, only choke that one voice
                if (event->note_id != -1) break;
            }
        }
    }
}

template <Scalar T, size_t MaxChannels, size_t NumVoices, typename VoiceType>
void Synthesizer<T, MaxChannels, NumVoices, VoiceType>::process(
    BufferView<T, MaxChannels> buffer,
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
                noteOn(note_event);
            } else if (header->type == CLAP_EVENT_NOTE_OFF) {
                const auto* note_event = reinterpret_cast<const clap_event_note_t*>(header);
                noteOff(note_event);
            } else if (header->type == CLAP_EVENT_NOTE_CHOKE) {
                const auto* note_event = reinterpret_cast<const clap_event_note_t*>(header);
                noteChoke(note_event);
            } else if (header->type == CLAP_EVENT_NOTE_EXPRESSION) {
                const auto* expr_event = reinterpret_cast<const clap_event_note_expression_t*>(header);
                // Apply expression to all matching voices (supports wildcards)
                for (auto& voice : voices_) {
                    if (voice.active_ && voice.note_.matches(expr_event->key, expr_event->note_id,
                                                             expr_event->port_index, expr_event->channel)) {
                        // Cast from CLAP expression ID to our enum (values match by design)
                        auto expression_id = static_cast<Note::Expression>(expr_event->expression_id);
                        // Update note data
                        voice.note_.applyExpression(expression_id, expr_event->value);
                        // Notify voice so it can update cached values (e.g. phase increment)
                        voice.onExpressionChange(expression_id, expr_event->value);
                    }
                }
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
} // namespace applause
