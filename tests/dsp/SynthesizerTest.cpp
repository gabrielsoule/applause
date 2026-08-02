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
    void process(BufferType, int, int) override {}
    void noteOff(bool) override { terminateVoice(); }
};

struct DispatchedVoice : applause::SynthesizerVoice<float> {
    void process(BufferType, int, int) override { ++process_calls; }

    int process_calls = 0;
};

struct CallbackVoice : applause::SynthesizerVoice<float> {
    void process(BufferType, int, int) override {}

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
    applause::Note::Expression last_expression = applause::Note::Volume;
    double last_expression_value = 0.0;
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
    synth.noteOn(&first_note);
    synth.noteOn(&second_note);

    std::array<float, 16> left_samples{};
    std::array<float, 16> right_samples{};
    std::array<float*, 2> channels{left_samples.data(), right_samples.data()};
    applause::BufferView<float> buffer{channels.data(), channels.size(), left_samples.size()};
    synth.process(buffer, nullptr);

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

    synth.noteOn(&first_note);
    synth.noteOff(&first_note_off);

    const auto voices = synth.getVoices();
    REQUIRE_FALSE(voices[0].active_);
    REQUIRE(voices[0].state_ == Voice::State::Idle);

    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 2, 64);
    synth.noteOn(&second_note);

    REQUIRE(voices[0].active_);
    REQUIRE(voices[0].state_ == Voice::State::KeyDown);
    REQUIRE(voices[0].note_.note_id == 2);
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
