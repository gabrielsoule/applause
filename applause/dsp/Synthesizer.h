#pragma once
#include <applause/core/ProcessInfo.h>
#include <applause/util/SampleType.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include <clap/events.h>

#include <applause/dsp/BufferView.h>
#include <applause/dsp/Note.h>

namespace applause {
/**
 * Base class for a reusable synthesizer voice.
 *
 * @tparam T The sample type of the voice (float or double).
 *
 * All voices live in a pool where they can be reused indefinitely.
 */

template <Scalar T>
class SynthesizerVoice {
public:
    using SampleType = T;
    using BufferType = BufferView<T>;

    virtual ~SynthesizerVoice() = default;

    virtual void process(BufferType buffer, int start_sample,
                         int num_samples) = 0;

    /**
     * Called on each active voice before the synthesizer prepares shared state
     * for the next processing block. This is useful for updating modulation state or doing any other sub-block-rate work. Released voices remain active until terminated. 
     */
    virtual void onPreProcess() noexcept {}

    /**
     * Called after preprocessing. note_ includes the final values from every
     * expression event at this sample. If note-on and note-off share a sample,
     * state_ is already Released and noteOff(false) follows this callback.
     */
    virtual void noteOn() {}

    /**
     * Called when this voice's MIDI note is released.
     *
     * This function can also be called when the voice is to be stolen.
     *
     * Same-sample events are coalesced, so an immediate termination may be
     * requested for a transient voice before its noteOn() callback has run.
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
     * Multiple updates to one expression at the same sample produce one
     * callback with the final value.
     * @param expression_id The expression that changed (Note::Expression::Tuning, etc.)
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
    virtual void onExpressionChange(Note::Expression, double) {}

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
        Idle,  // the voice is idle and ready to play a note
        KeyDown,  // the note is actively playing a note, and the corresponding key is down
        Sustained,  // the voice is no longer playing a note, but the sustain pedal is still down
        Released,  // the voice is still playing a note, but the corresponding key is released (e.g. release phase)
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
The maximum number of voices is fixed at compile time.

@tparam Voice The concrete voice class
@tparam NumVoices Maximum number of polyphonic voices
*/
template <typename Voice, std::size_t NumVoices = 16>
class Synthesizer {
public:
    using VoiceType = Voice;
    using SampleType = typename VoiceType::SampleType;
    using BufferType = typename VoiceType::BufferType;
    using VoiceBase = SynthesizerVoice<SampleType>;

    static_assert(Scalar<SampleType>,
                  "Synthesizer voice sample type must be float or double");
    static_assert(std::is_convertible_v<VoiceType*, VoiceBase*>,
                  "Voice must publicly derive from SynthesizerVoice<SampleType>");
    static_assert(std::is_same_v<BufferType, BufferView<SampleType>>,
                  "Voice BufferType must be BufferView<SampleType>");
    static_assert(std::is_default_constructible_v<VoiceType>,
                  "Voice must be concrete and default-constructible");
    static_assert(NumVoices > 0, "Synthesizer requires at least one voice");

    Synthesizer() = default;
    Synthesizer(const Synthesizer&) = default;
    Synthesizer(Synthesizer&&) = default;
    Synthesizer& operator=(const Synthesizer&) = default;
    Synthesizer& operator=(Synthesizer&&) = default;
    virtual ~Synthesizer() = default;

    [[nodiscard]] std::size_t getNumVoices() const noexcept {
        return NumVoices;
    }

    void activate(ProcessInfo info);
    void process(BufferType buffer, const clap_input_events_t* events);
    [[nodiscard]] std::span<VoiceType> getVoices() noexcept { return voices_; }

protected:
    /**
     * Called after active voice onPreProcess() hooks, before pending voice event
     * callbacks and rendering. Note expressions at this sample have been applied.
     * When the voice pool is full, pending releases are also prepared and dispatched
     * before allocating another note, so this hook can run more than once per sample.
     *
     * This is a good time to update values from parameters, evaluate modulation graphs, prepare voices, etc.
     *
     * This method may be called without any following samples, since event-only
     * and block-end updates are flushed immediately.
     */
    virtual void onPreProcess() noexcept {}

    /**
     * Renders one non-empty, event-stable range of the current process block.
     *
     * The default implementation processes each active voice independently.
     * Override this to use a different rendering strategy, such as processing
     * several logical voices together in SIMD lanes. process() clears the
     * output before the first call, and all events at start_sample have already
     * been applied. Implementations should render only the range
     * [start_sample, start_sample + num_samples).
     */
    virtual void renderSubBlock(BufferType buffer, int start_sample,
                                int num_samples);

private:
    void preProcessRange() noexcept;
    VoiceType& findFreeVoice();
    VoiceType& stealVoice();

    std::array<VoiceType, NumVoices> voices_;
    int notes_played_ = 0;  // count the number of notes; used for finding the
    // oldest voice during voice stealing
};

template <typename Voice, std::size_t NumVoices>
void Synthesizer<Voice, NumVoices>::preProcessRange() noexcept {
    for (auto& voice : voices_) {
        auto& state = static_cast<VoiceBase&>(voice);
        if (state.active_) state.onPreProcess();
    }
    onPreProcess();
}

template <typename Voice, std::size_t NumVoices>
void Synthesizer<Voice, NumVoices>::activate(ProcessInfo info) {
    for (auto& voice : voices_) {
        static_cast<VoiceBase&>(voice).setSampleRate(info.sample_rate);
    }
}

template <typename Voice, std::size_t NumVoices>
Voice& Synthesizer<Voice, NumVoices>::findFreeVoice() {
    for (auto& voice : voices_) {
        const auto& state = static_cast<const VoiceBase&>(voice);
        if (!state.active_ || state.state_ == VoiceBase::State::Idle) {
            return voice;
        }
    }

    return stealVoice();
}

template <typename Voice, std::size_t NumVoices>
Voice& Synthesizer<Voice, NumVoices>::stealVoice() {
    Voice* oldest = &voices_[0];
    for (auto& voice : voices_) {
        const auto& state = static_cast<const VoiceBase&>(voice);
        const auto& oldest_state = static_cast<const VoiceBase&>(*oldest);
        if (state.play_order_ < oldest_state.play_order_) {
            oldest = &voice;
        }
    }

    static_cast<VoiceBase&>(*oldest).noteOff(true);
    return *oldest;
}

template <typename Voice, std::size_t NumVoices>
void Synthesizer<Voice, NumVoices>::renderSubBlock(
    typename Voice::BufferType buffer, int start_sample, int num_samples) {
    for (auto& voice : voices_) {
        auto& state = static_cast<VoiceBase&>(voice);
        if (state.active_) {
            state.process(buffer, start_sample, num_samples);
        }
    }
}

template <typename Voice, std::size_t NumVoices>
void Synthesizer<Voice, NumVoices>::process(
    typename Voice::BufferType buffer, const clap_input_events_t* events) {
    buffer.clear();

    const uint32_t total_frames = buffer.numFrames();
    uint32_t current_sample = 0;
    bool range_prepared = false;
    const uint32_t event_count = events ? events->size(events) : 0;
    uint32_t event_index = 0;

    const auto isSupported = [](const clap_event_header_t* header) noexcept {
        if (!header || header->space_id != CLAP_CORE_EVENT_SPACE_ID) return false;
        return header->type == CLAP_EVENT_NOTE_ON ||
            header->type == CLAP_EVENT_NOTE_OFF ||
            header->type == CLAP_EVENT_NOTE_CHOKE ||
            header->type == CLAP_EVENT_NOTE_EXPRESSION;
    };

    static constexpr std::size_t expression_count =
        static_cast<std::size_t>(Note::Expression::Pressure) + 1;
    struct PendingCallbacks {
        bool note_on = false;
        bool note_off = false;
        std::array<const clap_event_note_expression_t*, expression_count> expressions{};
    };

    while (event_index < event_count) {
        while (event_index < event_count &&
               !isSupported(events->get(events, event_index)))
            ++event_index;
        if (event_index == event_count) break;

        const auto* first_header = events->get(events, event_index);
        const uint32_t event_time = std::min(first_header->time, total_frames);
        const uint32_t group_begin = event_index;
        uint32_t group_end = group_begin + 1;
        while (group_end < event_count) {
            const auto* header = events->get(events, group_end);
            if (isSupported(header) &&
                std::min(header->time, total_frames) != event_time)
                break;
            ++group_end;
        }

        if (event_time > current_sample) {
            if (!range_prepared) {
                preProcessRange();
                range_prepared = true;
            }
            renderSubBlock(buffer, static_cast<int>(current_sample),
                           static_cast<int>(event_time - current_sample));
        }

        std::array<PendingCallbacks, NumVoices> pending{};
        bool expressions_applied = false;

        const auto flushCallbacks = [&](bool releases_only) {
            for (uint32_t i = group_begin; i < group_end; ++i) {
                const auto* header = events->get(events, i);
                if (!isSupported(header) || header->type != CLAP_EVENT_NOTE_EXPRESSION)
                    continue;

                const auto* event = reinterpret_cast<const clap_event_note_expression_t*>(header);
                if (event->expression_id < 0 ||
                    static_cast<std::size_t>(event->expression_id) >= expression_count)
                    continue;

                const auto expression = static_cast<Note::Expression>(event->expression_id);
                for (std::size_t slot = 0; slot < NumVoices; ++slot) {
                    auto& state = static_cast<VoiceBase&>(voices_[slot]);
                    auto& callbacks = pending[slot];
                    if (expressions_applied && !callbacks.note_on) continue;
                    if (state.active_ &&
                        state.note_.matches(event->key, event->note_id, event->port_index, event->channel)) {
                        state.note_.applyExpression(expression, event->value);
                        callbacks.expressions[event->expression_id] = event;
                    }
                }
            }
            expressions_applied = true;
            preProcessRange();

            for (std::size_t slot = 0; slot < NumVoices; ++slot) {
                auto& state = static_cast<VoiceBase&>(voices_[slot]);
                auto& callbacks = pending[slot];
                if (releases_only && !callbacks.note_off) continue;

                if (callbacks.note_on && state.active_) state.noteOn();
                for (const auto* event : callbacks.expressions) {
                    if (!state.active_) break;
                    if (event)
                        state.onExpressionChange(static_cast<Note::Expression>(event->expression_id), event->value);
                }
                if (callbacks.note_off && state.active_) state.noteOff(false);
                callbacks = {};
            }
        };

        // Apply lifecycle changes in order, flushing releases only when allocation
        // needs them. Each flush includes all expressions at this sample.
        for (uint32_t i = group_begin; i < group_end; ++i) {
            const auto* header = events->get(events, i);
            if (!isSupported(header)) continue;

            if (header->type == CLAP_EVENT_NOTE_ON) {
                if (std::any_of(pending.begin(), pending.end(),
                                [](const auto& callbacks) { return callbacks.note_off; }) &&
                    std::all_of(voices_.begin(), voices_.end(), [](const VoiceBase& voice) {
                        return voice.active_ && voice.state_ != VoiceBase::State::Idle;
                    }))
                    flushCallbacks(true);

                const auto* note_event =
                    reinterpret_cast<const clap_event_note_t*>(header);
                auto& voice = findFreeVoice();
                const auto slot = static_cast<std::size_t>(&voice - voices_.data());
                auto& state = static_cast<VoiceBase&>(voice);
                pending[slot] = {};
                state.note_ = Note::fromNoteOn(note_event);
                state.play_order_ = notes_played_++;
                state.state_ = VoiceBase::State::KeyDown;
                state.active_ = true;
                pending[slot].note_on = true;
            } else if (header->type == CLAP_EVENT_NOTE_OFF) {
                const auto* note_event =
                    reinterpret_cast<const clap_event_note_t*>(header);
                for (std::size_t slot = 0; slot < NumVoices; ++slot) {
                    auto& state = static_cast<VoiceBase&>(voices_[slot]);
                    if (state.active_ && state.state_ == VoiceBase::State::KeyDown &&
                        state.note_.matches(note_event->key, note_event->note_id,
                                            note_event->port_index, note_event->channel)) {
                        state.note_.setNoteOff(note_event);
                        state.state_ = VoiceBase::State::Released;
                        pending[slot].note_off = true;
                        if (note_event->note_id != -1) break;
                    }
                }
            } else if (header->type == CLAP_EVENT_NOTE_CHOKE) {
                const auto* note_event =
                    reinterpret_cast<const clap_event_note_t*>(header);
                for (std::size_t slot = 0; slot < NumVoices; ++slot) {
                    auto& state = static_cast<VoiceBase&>(voices_[slot]);
                    if (state.active_ &&
                        state.note_.matches(note_event->key, note_event->note_id,
                                            note_event->port_index, note_event->channel)) {
                        state.noteOff(true);
                        pending[slot] = {};
                        if (note_event->note_id != -1) break;
                    }
                }
            }
        }

        flushCallbacks(false);
        range_prepared = true;

        current_sample = event_time;
        event_index = group_end;
    }

    if (current_sample < total_frames) {
        if (!range_prepared) preProcessRange();
        renderSubBlock(buffer, static_cast<int>(current_sample),
                       static_cast<int>(total_frames - current_sample));
    }
}
}  // namespace applause
