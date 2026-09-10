#include <applause/dsp/Synthesizer.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <type_traits>
#include <vector>

namespace {
clap_event_note_t makeNoteEvent(uint16_t type, uint32_t time, int32_t note_id, int16_t key) {
    clap_event_note_t event{};
    event.header.size = sizeof(event);
    event.header.time = time;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = type;
    event.note_id = note_id;
    event.port_index = 0;
    event.channel = 0;
    event.key = key;
    event.velocity = 1.0;
    return event;
}

clap_event_note_expression_t makeExpressionEvent(uint32_t time, int32_t note_id,
                                                 int16_t key,
                                                 clap_note_expression expression,
                                                 double value) {
    clap_event_note_expression_t event{};
    event.header.size = sizeof(event);
    event.header.time = time;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_NOTE_EXPRESSION;
    event.expression_id = expression;
    event.note_id = note_id;
    event.port_index = 0;
    event.channel = 0;
    event.key = key;
    event.value = value;
    return event;
}

clap_event_param_value_t makeParamEvent(uint32_t time) {
    clap_event_param_value_t event{};
    event.header.size = sizeof(event);
    event.header.time = time;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    return event;
}

struct EventList {
    std::vector<const clap_event_header_t*> events;
    clap_input_events_t input{
        .ctx = this,
        .size = [](const clap_input_events_t* list) -> uint32_t {
            return static_cast<uint32_t>(
                static_cast<EventList*>(list->ctx)->events.size());
        },
        .get = [](const clap_input_events_t* list, uint32_t index) -> const clap_event_header_t* {
            return static_cast<EventList*>(list->ctx)->events[index];
        },
    };

    template <typename Event>
    void add(const Event& event) {
        events.push_back(&event.header);
    }
};

struct CountingVoice : applause::SynthesizerVoice<float> {
    void process(BufferType buffer, int start_sample,
                 int num_samples) override {
        ++process_calls;
        for (std::size_t channel = 0; channel < buffer.numChannels(); ++channel) {
            for (int i = 0; i < num_samples; ++i)
                buffer.add(channel, static_cast<std::size_t>(start_sample + i), 1.0f);
        }
    }

    int process_calls = 0;
};

struct SynchronouslyTerminatingVoice : applause::SynthesizerVoice<float> {
    void process(BufferType, int, int) override { ++process_calls; }
    void noteOff(bool) override { terminateVoice(); }

    int process_calls = 0;
};

struct PreProcessTerminatingVoice : applause::SynthesizerVoice<float> {
    void process(BufferType, int, int) override { ++process_calls; }
    void onPreProcess() noexcept override { terminateVoice(); }
    void noteOn() override { ++note_on_calls; }

    int process_calls = 0;
    int note_on_calls = 0;
};

struct DispatchedVoice : applause::SynthesizerVoice<float> {
    void process(BufferType, int, int) override { ++process_calls; }

    int process_calls = 0;
};

struct CallbackVoice : applause::SynthesizerVoice<float> {
    void process(BufferType, int, int) override { ++process_calls; }

    void noteOn() override { ++note_on_calls; }

    void noteOff(bool terminate_now) override {
        if (terminate_now) {
            ++immediate_note_off_calls;
        } else {
            ++note_off_calls;
        }
        applause::SynthesizerVoice<float>::noteOff(terminate_now);
    }

    void onExpressionChange(applause::Note::Expression expression_id,
                            double value) override {
        ++expression_calls;
        last_expression = expression_id;
        last_expression_value = value;
    }

    int note_on_calls = 0;
    int note_off_calls = 0;
    int immediate_note_off_calls = 0;
    int expression_calls = 0;
    int process_calls = 0;
    applause::Note::Expression last_expression = applause::Note::Volume;
    double last_expression_value = 0.0;
};

struct PressureReleaseVoice : CallbackVoice {
    void onPreProcess() noexcept override { release_pressure = note_.pressure; }

    void noteOff(bool terminate_now) override {
        CallbackVoice::noteOff(terminate_now);
        if (!terminate_now && release_pressure >= 0.5) terminateVoice();
    }

    double release_pressure = 0.0;
};

struct MissingProcessVoice : applause::SynthesizerVoice<float> {};

class AggregateTestSynth final : public applause::Synthesizer<DispatchedVoice, 4> {
public:
    struct Chunk {
        int start = 0;
        int count = 0;
    };

    std::array<Chunk, 8> chunks{};
    size_t chunk_count = 0;

protected:
    void renderSubBlock(BufferType buffer, int start_sample,
                        int num_samples) override {
        chunks[chunk_count++] = {.start = start_sample, .count = num_samples};
        for (int i = 0; i < num_samples; ++i)
            buffer.add(0, static_cast<std::size_t>(start_sample + i), 1.0f);
    }
};

struct PreProcessVoice : applause::SynthesizerVoice<float> {
    struct Snapshot {
        bool active;
        State state;
        int32_t note_id;
        double pressure;
        double note_off_velocity;
    };

    void process(BufferType, int, int) override {}
    void onPreProcess() noexcept override {
        trace->push_back('V');
        snapshots->push_back({active_, state_, note_.note_id,
                              note_.pressure, note_.note_off_velocity});
    }
    void noteOn() override { trace->push_back('N'); }
    void noteOff(bool terminate_now) override {
        trace->push_back(terminate_now ? 'K' : 'F');
        applause::SynthesizerVoice<float>::noteOff(terminate_now);
    }
    void onExpressionChange(applause::Note::Expression expression,
                            double value) override {
        trace->push_back('E');
        ++expression_calls;
        last_expression = expression;
        last_expression_value = value;
    }

    std::vector<char>* trace = nullptr;
    std::vector<Snapshot>* snapshots = nullptr;
    int expression_calls = 0;
    applause::Note::Expression last_expression = applause::Note::Volume;
    double last_expression_value = 0.0;
};

class PreProcessTestSynth final : public applause::Synthesizer<PreProcessVoice, 1> {
public:
    using VoiceState = applause::SynthesizerVoice<float>::State;

    using Snapshot = PreProcessVoice::Snapshot;

    PreProcessTestSynth() {
        getVoices().front().trace = &trace;
        getVoices().front().snapshots = &voice_snapshots;
    }

    std::vector<char> trace;
    std::vector<Snapshot> snapshots;
    std::vector<Snapshot> voice_snapshots;

protected:
    void onPreProcess() noexcept override {
        trace.push_back('P');
        const auto& voice = getVoices().front();
        snapshots.push_back({voice.active_, voice.state_, voice.note_.note_id,
                             voice.note_.pressure, voice.note_.note_off_velocity});
    }

    void renderSubBlock(BufferType, int, int) override { trace.push_back('R'); }
};

static_assert(std::is_abstract_v<applause::SynthesizerVoice<float>>);
static_assert(
    std::has_virtual_destructor_v<applause::SynthesizerVoice<float>>);
static_assert(std::is_abstract_v<MissingProcessVoice>);
static_assert(!std::is_abstract_v<CountingVoice>);
static_assert(std::is_polymorphic_v<CountingVoice>);
}  // namespace

TEST_CASE("Synthesizer default renderer processes runtime-sized stereo buffers", "[dsp][synthesizer]") {
    applause::Synthesizer<CountingVoice, 4> synth;
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 2, 64);
    EventList event_list;
    event_list.add(first_note);
    event_list.add(second_note);

    std::array<float, 16> left_samples{};
    std::array<float, 16> right_samples{};
    std::array<float*, 2> channels{left_samples.data(), right_samples.data()};
    applause::BufferView<float> buffer{channels.data(), channels.size(), left_samples.size()};
    synth.process(buffer, &event_list.input);

    for (const auto* channel : channels) {
        for (std::size_t frame = 0; frame < left_samples.size(); ++frame)
            REQUIRE(channel[frame] == 2.0f);
    }

    const auto voices = synth.getVoices();
    REQUIRE(voices[0].process_calls == 1);
    REQUIRE(voices[1].process_calls == 1);
    REQUIRE(voices[2].process_calls == 0);
    REQUIRE(voices[3].process_calls == 0);
}

TEST_CASE("Synthesizer keeps a synchronously terminated note-off voice idle", "[dsp][synthesizer]") {
    using Voice = applause::SynthesizerVoice<float>;

    applause::Synthesizer<SynchronouslyTerminatingVoice, 1> synth;
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    auto first_note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 0, 1, 60);
    EventList first_events;
    first_events.add(first_note);
    first_events.add(first_note_off);

    std::array<float, 1> samples{};
    std::array<float*, 1> channels{samples.data()};
    applause::BufferView<float> buffer{channels.data(), channels.size(), samples.size()};
    synth.process(buffer, &first_events.input);

    const auto voices = synth.getVoices();
    REQUIRE_FALSE(voices[0].active_);
    REQUIRE(voices[0].state_ == Voice::State::Idle);
    REQUIRE(voices[0].process_calls == 0);

    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 2, 64);
    EventList second_events;
    second_events.add(second_note);
    synth.process(buffer, &second_events.input);

    REQUIRE(voices[0].active_);
    REQUIRE(voices[0].state_ == Voice::State::KeyDown);
    REQUIRE(voices[0].note_.note_id == 2);
}

TEST_CASE("Synthesizer skips callbacks and rendering after preprocessing terminates a voice",
          "[dsp][synthesizer]") {
    applause::Synthesizer<PreProcessTerminatingVoice, 1> synth;
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    EventList event_list;
    event_list.add(note_on);

    std::array<float, 1> samples{};
    std::array<float*, 1> channels{samples.data()};
    synth.process({channels.data(), channels.size(), samples.size()},
                  &event_list.input);

    const auto& voice = synth.getVoices().front();
    REQUIRE_FALSE(voice.active_);
    REQUIRE(voice.note_on_calls == 0);
    REQUIRE(voice.process_calls == 0);
}

TEST_CASE("Synthesizer reuses a same-sample release before stealing another voice",
          "[dsp][synthesizer]") {
    applause::Synthesizer<PressureReleaseVoice, 2> synth;
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    EventList first_events;
    first_events.add(first_note);
    synth.process({}, &first_events.input);

    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 2, 64);
    auto note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 0, 2, 64);
    auto replacement = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 3, 67);
    auto pressure = makeExpressionEvent(
        0, 2, 64, CLAP_NOTE_EXPRESSION_PRESSURE, 0.75);
    EventList release_events;

    SECTION("the released note was already playing") {
        EventList second_events;
        second_events.add(second_note);
        synth.process({}, &second_events.input);
    }

    SECTION("the released note starts at the same sample") {
        release_events.add(second_note);
    }

    release_events.add(note_off);
    release_events.add(replacement);
    release_events.add(pressure);
    synth.process({}, &release_events.input);

    const auto voices = synth.getVoices();
    REQUIRE(voices[0].active_);
    REQUIRE(voices[0].note_.note_id == 1);
    REQUIRE(voices[0].immediate_note_off_calls == 0);
    REQUIRE(voices[1].active_);
    REQUIRE(voices[1].state_ == PressureReleaseVoice::State::KeyDown);
    REQUIRE(voices[1].note_.note_id == 3);
    REQUIRE(voices[1].note_on_calls == 2);
    REQUIRE(voices[1].note_off_calls == 1);
    REQUIRE(voices[1].immediate_note_off_calls == 0);
    REQUIRE(voices[1].expression_calls == 1);
    REQUIRE(voices[1].last_expression_value == 0.75);
    REQUIRE(voices[1].note_.pressure == 0.0);
}

TEST_CASE("Synthesizer keeps a release tail active when allocating a same-sample note",
          "[dsp][synthesizer]") {
    applause::Synthesizer<PressureReleaseVoice, 2> synth;
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 2, 64);
    EventList first_events;
    first_events.add(first_note);
    first_events.add(second_note);
    synth.process({}, &first_events.input);

    auto note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 0, 2, 64);
    auto replacement = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 3, 67);
    auto pressure = makeExpressionEvent(
        0, 2, 64, CLAP_NOTE_EXPRESSION_PRESSURE, 0.25);
    EventList release_events;
    release_events.add(note_off);
    release_events.add(replacement);
    release_events.add(pressure);
    synth.process({}, &release_events.input);

    const auto voices = synth.getVoices();
    REQUIRE(voices[0].active_);
    REQUIRE(voices[0].note_.note_id == 3);
    REQUIRE(voices[0].immediate_note_off_calls == 1);
    REQUIRE(voices[1].active_);
    REQUIRE(voices[1].state_ == PressureReleaseVoice::State::Released);
    REQUIRE(voices[1].note_.note_id == 2);
    REQUIRE(voices[1].note_off_calls == 1);
    REQUIRE(voices[1].immediate_note_off_calls == 0);
    REQUIRE(voices[1].expression_calls == 1);
    REQUIRE(voices[1].release_pressure == 0.25);
}

TEST_CASE("Synthesizer reuses a slot repeatedly at the same sample",
          "[dsp][synthesizer]") {
    applause::Synthesizer<PressureReleaseVoice, 2> synth;
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 2, 64);
    EventList first_events;
    first_events.add(first_note);
    first_events.add(second_note);
    synth.process({}, &first_events.input);

    const std::array notes{
        makeNoteEvent(CLAP_EVENT_NOTE_OFF, 0, 2, 64),
        makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 3, 65),
        makeNoteEvent(CLAP_EVENT_NOTE_OFF, 0, 3, 65),
        makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 4, 67),
        makeNoteEvent(CLAP_EVENT_NOTE_OFF, 0, 4, 67),
        makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 5, 69),
    };
    const std::array expressions{
        makeExpressionEvent(0, 1, 60, CLAP_NOTE_EXPRESSION_PRESSURE, 0.25),
        makeExpressionEvent(0, 2, 64, CLAP_NOTE_EXPRESSION_PRESSURE, 0.75),
        makeExpressionEvent(0, 3, 65, CLAP_NOTE_EXPRESSION_PRESSURE, 0.75),
        makeExpressionEvent(0, 4, 67, CLAP_NOTE_EXPRESSION_PRESSURE, 0.75),
    };
    EventList replacement_events;
    for (const auto& note : notes) replacement_events.add(note);
    for (const auto& expression : expressions) replacement_events.add(expression);
    synth.process({}, &replacement_events.input);

    const auto voices = synth.getVoices();
    REQUIRE(voices[0].active_);
    REQUIRE(voices[0].note_.note_id == 1);
    REQUIRE(voices[0].immediate_note_off_calls == 0);
    REQUIRE(voices[0].expression_calls == 1);
    REQUIRE(voices[0].last_expression_value == 0.25);
    REQUIRE(voices[1].active_);
    REQUIRE(voices[1].state_ == PressureReleaseVoice::State::KeyDown);
    REQUIRE(voices[1].note_.note_id == 5);
    REQUIRE(voices[1].note_on_calls == 4);
    REQUIRE(voices[1].note_off_calls == 3);
    REQUIRE(voices[1].immediate_note_off_calls == 0);
    REQUIRE(voices[1].expression_calls == 3);
    REQUIRE(voices[1].note_.pressure == 0.0);
}

TEST_CASE("Synthesizer keeps unrelated starts deferred while settling a release",
          "[dsp][synthesizer]") {
    applause::Synthesizer<PressureReleaseVoice, 2> synth;
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    EventList first_events;
    first_events.add(first_note);
    synth.process({}, &first_events.input);

    auto transient_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 2, 64);
    auto note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 0, 1, 60);
    auto replacement = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 3, 67);
    auto choke = makeNoteEvent(CLAP_EVENT_NOTE_CHOKE, 0, 2, 64);
    auto pressure = makeExpressionEvent(
        0, 1, 60, CLAP_NOTE_EXPRESSION_PRESSURE, 0.75);
    EventList replacement_events;
    replacement_events.add(transient_note);
    replacement_events.add(note_off);
    replacement_events.add(replacement);
    replacement_events.add(choke);
    replacement_events.add(pressure);
    synth.process({}, &replacement_events.input);

    const auto voices = synth.getVoices();
    REQUIRE(voices[0].active_);
    REQUIRE(voices[0].note_.note_id == 3);
    REQUIRE(voices[0].note_on_calls == 2);
    REQUIRE(voices[0].note_off_calls == 1);
    REQUIRE(voices[0].immediate_note_off_calls == 0);
    REQUIRE_FALSE(voices[1].active_);
    REQUIRE(voices[1].note_on_calls == 0);
    REQUIRE(voices[1].immediate_note_off_calls == 1);
}

TEST_CASE("Synthesizer dispatches virtual voice callbacks", "[dsp][synthesizer]") {
    applause::Synthesizer<CallbackVoice, 2> synth;
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    auto expression = makeExpressionEvent(
        1, 1, 60, CLAP_NOTE_EXPRESSION_PRESSURE, 0.75);
    auto first_note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 2, 1, 60);
    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 3, 2, 64);
    auto second_note_choke =
        makeNoteEvent(CLAP_EVENT_NOTE_CHOKE, 4, 2, 64);

    EventList event_list;
    event_list.add(first_note);
    event_list.add(expression);
    event_list.add(first_note_off);
    event_list.add(second_note);
    event_list.add(second_note_choke);

    std::array<float, 8> samples{};
    std::array<float*, 1> channels{samples.data()};
    applause::BufferView<float> buffer{channels.data(), channels.size(),
                                        samples.size()};
    synth.process(buffer, &event_list.input);

    const auto voices = synth.getVoices();
    REQUIRE(voices[0].note_on_calls == 1);
    REQUIRE(voices[0].note_off_calls == 1);
    REQUIRE(voices[0].immediate_note_off_calls == 0);
    REQUIRE(voices[0].expression_calls == 1);
    REQUIRE(voices[0].last_expression == applause::Note::Pressure);
    REQUIRE(voices[0].last_expression_value == 0.75);
    REQUIRE(voices[0].note_.pressure == 0.75);
    REQUIRE(voices[0].active_);
    REQUIRE(voices[0].state_ == CallbackVoice::State::Released);

    REQUIRE(voices[1].note_on_calls == 1);
    REQUIRE(voices[1].note_off_calls == 0);
    REQUIRE(voices[1].immediate_note_off_calls == 1);
    REQUIRE(voices[1].expression_calls == 0);
    REQUIRE_FALSE(voices[1].active_);
    REQUIRE(voices[1].state_ == CallbackVoice::State::Idle);
}

TEST_CASE("Synthesizer calls onPreProcess once per event-stable DSP range",
          "[dsp][synthesizer]") {
    PreProcessTestSynth synth;
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 1, 60);
    auto expression = makeExpressionEvent(3, 1, 60, CLAP_NOTE_EXPRESSION_PRESSURE, 0.75);
    auto note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 5, 1, 60);
    note_off.velocity = 0.25;

    EventList event_list;
    event_list.add(note_on);
    event_list.add(expression);
    event_list.add(note_off);

    std::array<float, 7> samples{};
    std::array<float*, 1> channels{samples.data()};
    synth.process({channels.data(), channels.size(), samples.size()}, &event_list.input);

    const std::vector<char> expected_trace{'P', 'R', 'V', 'P', 'N', 'R',
                                           'V', 'P', 'E', 'R', 'V', 'P', 'F', 'R'};
    REQUIRE(synth.trace == expected_trace);
    REQUIRE(synth.snapshots.size() == 4);
    REQUIRE(synth.voice_snapshots.size() == 3);

    REQUIRE_FALSE(synth.snapshots[0].active);
    REQUIRE(synth.snapshots[1].active);
    REQUIRE(synth.snapshots[1].state == PreProcessTestSynth::VoiceState::KeyDown);
    REQUIRE(synth.snapshots[1].note_id == 1);
    REQUIRE(synth.snapshots[2].pressure == 0.75);
    REQUIRE(synth.snapshots[3].note_off_velocity == 0.25);
    REQUIRE(synth.snapshots[3].state == PreProcessTestSynth::VoiceState::Released);

    REQUIRE(synth.voice_snapshots[0].state == PreProcessTestSynth::VoiceState::KeyDown);
    REQUIRE(synth.voice_snapshots[0].note_id == 1);
    REQUIRE(synth.voice_snapshots[1].pressure == 0.75);
    REQUIRE(synth.voice_snapshots[2].note_off_velocity == 0.25);
    REQUIRE(synth.voice_snapshots[2].state == PreProcessTestSynth::VoiceState::Released);
}

TEST_CASE("Synthesizer does not preprocess an empty range before a frame-zero event",
          "[dsp][synthesizer]") {
    PreProcessTestSynth synth;
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    EventList event_list;
    event_list.add(note_on);

    std::array<float, 1> samples{};
    std::array<float*, 1> channels{samples.data()};
    synth.process({channels.data(), channels.size(), samples.size()}, &event_list.input);

    const std::vector<char> expected_trace{'V', 'P', 'N', 'R'};
    REQUIRE(synth.trace == expected_trace);
}

TEST_CASE("Synthesizer coalesces same-sample events before preprocessing",
          "[dsp][synthesizer]") {
    PreProcessTestSynth synth;
    auto first_pressure = makeExpressionEvent(
        0, 1, 60, CLAP_NOTE_EXPRESSION_PRESSURE, 0.25);
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    auto final_pressure = makeExpressionEvent(
        0, 1, 60, CLAP_NOTE_EXPRESSION_PRESSURE, 0.75);
    EventList event_list;
    event_list.add(first_pressure);
    event_list.add(note_on);
    event_list.add(final_pressure);

    std::array<float, 1> samples{};
    std::array<float*, 1> channels{samples.data()};
    synth.process({channels.data(), channels.size(), samples.size()},
                  &event_list.input);

    const std::vector<char> expected_trace{'V', 'P', 'N', 'E', 'R'};
    REQUIRE(synth.trace == expected_trace);
    REQUIRE(synth.voice_snapshots.size() == 1);
    REQUIRE(synth.voice_snapshots[0].state == PreProcessTestSynth::VoiceState::KeyDown);
    REQUIRE(synth.voice_snapshots[0].pressure == 0.75);
    REQUIRE(synth.snapshots.size() == 1);
    REQUIRE(synth.snapshots[0].pressure == 0.75);

    const auto& voice = synth.getVoices().front();
    REQUIRE(voice.expression_calls == 1);
    REQUIRE(voice.last_expression == applause::Note::Pressure);
    REQUIRE(voice.last_expression_value == 0.75);
}

TEST_CASE("Synthesizer starts and releases a same-sample note after one preparation",
          "[dsp][synthesizer]") {
    PreProcessTestSynth synth;
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    auto note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 0, 1, 60);
    EventList event_list;
    event_list.add(note_on);
    event_list.add(note_off);

    std::array<float, 1> samples{};
    std::array<float*, 1> channels{samples.data()};
    synth.process({channels.data(), channels.size(), samples.size()},
                  &event_list.input);

    const std::vector<char> expected_trace{'V', 'P', 'N', 'F', 'R'};
    REQUIRE(synth.trace == expected_trace);
    REQUIRE(synth.voice_snapshots.size() == 1);
    REQUIRE(synth.voice_snapshots[0].state == PreProcessTestSynth::VoiceState::Released);
}

TEST_CASE("Synthesizer flushes event-only and block-end state",
          "[dsp][synthesizer]") {
    SECTION("zero-frame event") {
        PreProcessTestSynth synth;
        auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
        EventList event_list;
        event_list.add(note_on);

        synth.process({}, &event_list.input);

        const std::vector<char> expected_trace{'V', 'P', 'N'};
        REQUIRE(synth.trace == expected_trace);
    }

    SECTION("block-end event") {
        PreProcessTestSynth synth;
        auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 4, 1, 60);
        EventList event_list;
        event_list.add(note_on);

        std::array<float, 4> samples{};
        std::array<float*, 1> channels{samples.data()};
        synth.process({channels.data(), channels.size(), samples.size()},
                      &event_list.input);

        const std::vector<char> expected_trace{'P', 'R', 'V', 'P', 'N'};
        REQUIRE(synth.trace == expected_trace);
    }
}

TEST_CASE("Synthesizer discards a same-sample note choked before preparation",
          "[dsp][synthesizer]") {
    applause::Synthesizer<CallbackVoice, 1> synth;
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    auto choke = makeNoteEvent(CLAP_EVENT_NOTE_CHOKE, 0, 1, 60);
    EventList event_list;
    event_list.add(note_on);
    event_list.add(choke);

    std::array<float, 1> samples{};
    std::array<float*, 1> channels{samples.data()};
    synth.process({channels.data(), channels.size(), samples.size()},
                  &event_list.input);

    const auto& voice = synth.getVoices().front();
    REQUIRE(voice.note_on_calls == 0);
    REQUIRE(voice.immediate_note_off_calls == 1);
    REQUIRE(voice.process_calls == 0);
    REQUIRE_FALSE(voice.active_);
}

TEST_CASE("Synthesizer cleans a stolen voice before preparing its replacement",
          "[dsp][synthesizer]") {
    applause::Synthesizer<CallbackVoice, 1> synth;
    std::array<float, 1> samples{};
    std::array<float*, 1> channels{samples.data()};

    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    EventList first_events;
    first_events.add(first_note);
    synth.process({channels.data(), channels.size(), samples.size()},
                  &first_events.input);

    auto replacement = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 2, 64);
    EventList replacement_events;
    replacement_events.add(replacement);
    synth.process({channels.data(), channels.size(), samples.size()},
                  &replacement_events.input);

    const auto& voice = synth.getVoices().front();
    REQUIRE(voice.note_on_calls == 2);
    REQUIRE(voice.immediate_note_off_calls == 1);
    REQUIRE(voice.active_);
    REQUIRE(voice.note_.note_id == 2);
}

TEST_CASE("Synthesizer ignores unsupported render boundaries", "[dsp][synthesizer]") {
    AggregateTestSynth synth;
    clap_event_header_t foreign_event{
        .size = sizeof(clap_event_header_t),
        .time = 2,
        .space_id = static_cast<uint16_t>(CLAP_CORE_EVENT_SPACE_ID + 1),
        .type = CLAP_EVENT_PARAM_VALUE,
    };
    auto first_param = makeParamEvent(3);
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 4, 7, 60);
    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 4, 8, 64);
    auto second_param = makeParamEvent(7);
    auto first_note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 10, 7, 60);
    auto third_param = makeParamEvent(12);
    EventList event_list;
    event_list.events.push_back(&foreign_event);
    event_list.events.push_back(nullptr);
    event_list.add(first_param);
    event_list.add(first_note);
    event_list.add(second_note);
    event_list.add(second_param);
    event_list.add(first_note_off);
    event_list.add(third_param);

    std::array<float, 16> samples{};
    std::array<float*, 1> channels{samples.data()};
    applause::BufferView<float> buffer{channels.data(), channels.size(), samples.size()};
    synth.process(buffer, &event_list.input);

    REQUIRE(synth.chunk_count == 3);
    REQUIRE(synth.chunks[0].start == 0);
    REQUIRE(synth.chunks[0].count == 4);
    REQUIRE(synth.chunks[1].start == 4);
    REQUIRE(synth.chunks[1].count == 6);
    REQUIRE(synth.chunks[2].start == 10);
    REQUIRE(synth.chunks[2].count == 6);

    for (const auto sample : samples)
        REQUIRE(sample == 1.0f);

    for (const auto& voice : synth.getVoices())
        REQUIRE(voice.process_calls == 0);
}

TEST_CASE("Synthesizer renders one range around ignored events", "[dsp][synthesizer]") {
    AggregateTestSynth synth;
    auto first_param = makeParamEvent(4);
    clap_event_header_t foreign_event{
        .size = sizeof(clap_event_header_t),
        .time = 8,
        .space_id = static_cast<uint16_t>(CLAP_CORE_EVENT_SPACE_ID + 1),
        .type = CLAP_EVENT_PARAM_VALUE,
    };
    auto second_param = makeParamEvent(12);
    EventList event_list;
    event_list.add(first_param);
    event_list.events.push_back(&foreign_event);
    event_list.events.push_back(nullptr);
    event_list.add(second_param);

    std::array<float, 16> samples{};
    std::array<float*, 1> channels{samples.data()};
    applause::BufferView<float> buffer{channels.data(), channels.size(),
                                        samples.size()};
    synth.process(buffer, &event_list.input);

    REQUIRE(synth.chunk_count == 1);
    REQUIRE(synth.chunks[0].start == 0);
    REQUIRE(synth.chunks[0].count == 16);
    for (const auto sample : samples)
        REQUIRE(sample == 1.0f);
}
