#include <applause/dsp/Synthesizer.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
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

struct NoteEventList {
    std::vector<clap_event_note_t> events;
    clap_input_events_t input{
        .ctx = this,
        .size = [](const clap_input_events_t* list) -> uint32_t {
            return static_cast<uint32_t>(static_cast<NoteEventList*>(list->ctx)->events.size());
        },
        .get = [](const clap_input_events_t* list, uint32_t index) -> const clap_event_header_t* {
            return &static_cast<NoteEventList*>(list->ctx)->events[index].header;
        },
    };
};

struct CountingVoice : applause::SynthesizerVoice<float, 1> {
    void process(applause::BufferView<float, 1> buffer, int start_sample, int num_samples) override {
        ++process_calls;
        for (int i = 0; i < num_samples; ++i)
            buffer.add(0, static_cast<size_t>(start_sample + i), 1.0f);
    }

    int process_calls = 0;
};

struct DispatchedVoice : applause::SynthesizerVoice<float, 1> {
    void process(applause::BufferView<float, 1>, int, int) override { ++process_calls; }

    int process_calls = 0;
};

class AggregateTestSynth final : public applause::Synthesizer<float, 1, 4, DispatchedVoice> {
public:
    struct Chunk {
        int start = 0;
        int count = 0;
    };

    std::array<Chunk, 8> chunks{};
    size_t chunk_count = 0;

protected:
    void renderSubBlock(applause::BufferView<float, 1> buffer, int start_sample, int num_samples) override {
        chunks[chunk_count++] = {.start = start_sample, .count = num_samples};
        for (int i = 0; i < num_samples; ++i)
            buffer.add(0, static_cast<size_t>(start_sample + i), 1.0f);
    }
};
}  // namespace

TEST_CASE("Synthesizer default sub-block renderer processes each active voice", "[dsp][synthesizer]") {
    applause::Synthesizer<float, 1, 4, CountingVoice> synth;
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 1, 60);
    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 0, 2, 64);
    synth.noteOn(&first_note);
    synth.noteOn(&second_note);

    std::array<float, 16> samples{};
    applause::BufferView<float, 1> buffer{samples.data(), 1, samples.size()};
    synth.process(buffer, nullptr);

    for (const auto sample : samples)
        REQUIRE(sample == 2.0f);

    const auto voices = synth.getVoices();
    REQUIRE(voices[0].process_calls == 1);
    REQUIRE(voices[1].process_calls == 1);
    REQUIRE(voices[2].process_calls == 0);
    REQUIRE(voices[3].process_calls == 0);
}

TEST_CASE("Synthesizer override renders once per event-stable sub-block", "[dsp][synthesizer]") {
    AggregateTestSynth synth;
    NoteEventList event_list;
    event_list.events.push_back(makeNoteEvent(CLAP_EVENT_NOTE_ON, 4, 7, 60));
    event_list.events.push_back(makeNoteEvent(CLAP_EVENT_NOTE_ON, 4, 8, 64));
    event_list.events.push_back(makeNoteEvent(CLAP_EVENT_NOTE_OFF, 10, 7, 60));

    std::array<float, 16> samples{};
    applause::BufferView<float, 1> buffer{samples.data(), 1, samples.size()};
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
