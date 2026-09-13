#include "SimdPolySynth.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
constexpr double sample_rate = 48000.0;
constexpr double two_pi = 6.28318530717958647692;
constexpr double voice_gain = 1.41421356237309504880;
constexpr double envelope_curve = -4.0;

double envelopeProgress(std::size_t samples, std::size_t total_samples) {
    if (total_samples == 0) return 1.0;
    return std::expm1(envelope_curve * static_cast<double>(samples) / static_cast<double>(total_samples)) /
        std::expm1(envelope_curve);
}

double centeredSaw(double phase, double increment) {
    phase -= std::floor(phase);
    auto output = 2.0 * phase - 1.0;
    if (phase < increment) {
        const auto distance = 1.0 - phase / increment;
        output += distance * distance;
    }
    if (phase > 1.0 - increment) {
        const auto distance = 1.0 - (1.0 - phase) / increment;
        output -= distance * distance;
    }
    return output;
}

double centeredSquare(double phase, double increment) {
    return centeredSaw(phase + 0.5, increment) - centeredSaw(phase, increment);
}

void registerOscillatorParameters(applause::ParamsExtension& params) {
    const auto registerOscillator = [&params](const std::string& prefix) {
        params.registerParam({.string_id = prefix + "_waveform",
                              .name = "Waveform",
                              .default_value = 0.0f,
                              .choices = {"SIN", "TRI", "SAW", "SQR"}});
        params.registerParam({.string_id = prefix + "_octave",
                              .name = "Octave",
                              .min_value = -2.0f,
                              .max_value = 2.0f,
                              .default_value = 0.0f,
                              .is_stepped = true,
                              .is_polyphonic = true});
        params.registerParam({.string_id = prefix + "_semitone",
                              .name = "Semitone",
                              .min_value = -12.0f,
                              .max_value = 12.0f,
                              .default_value = 0.0f,
                              .is_stepped = true,
                              .is_polyphonic = true});
        params.registerParam({.string_id = prefix + "_fine",
                              .name = "Fine",
                              .min_value = -100.0f,
                              .max_value = 100.0f,
                              .default_value = 0.0f,
                              .is_polyphonic = true});
        params.registerParam({.string_id = prefix + "_level",
                              .name = "Level",
                              .min_value = 0.0f,
                              .max_value = 1.0f,
                              .default_value = 1.0f,
                              .is_polyphonic = true});
        params.registerParam({.string_id = prefix + "_pan",
                              .name = "Pan",
                              .min_value = -1.0f,
                              .max_value = 1.0f,
                              .default_value = 0.0f,
                              .is_polyphonic = true});
    };
    registerOscillator("osc1");
    registerOscillator("osc2");
}

void registerEnvelopeParameters(applause::ParamsExtension& params, float attack, float decay, float sustain,
                                float release) {
    params.registerParam({.string_id = "attack",
                          .name = "Attack",
                          .min_value = 0.0f,
                          .max_value = 1.0f,
                          .default_value = attack,
                          .is_polyphonic = true});
    params.registerParam({.string_id = "decay",
                          .name = "Decay",
                          .min_value = 0.0f,
                          .max_value = 1.0f,
                          .default_value = decay,
                          .is_polyphonic = true});
    params.registerParam({.string_id = "sustain",
                          .name = "Sustain",
                          .min_value = 0.0f,
                          .max_value = 1.0f,
                          .default_value = sustain,
                          .is_polyphonic = true});
    params.registerParam({.string_id = "release",
                          .name = "Release",
                          .min_value = 0.0f,
                          .max_value = 1.0f,
                          .default_value = release,
                          .is_polyphonic = true});
}

BatchedVoice::Mask laneMask(std::uint64_t bits) { return BatchedVoice::Mask::from_mask(bits); }

clap_event_note_t makeNoteEvent(uint16_t type, int32_t note_id, int16_t key, uint32_t time = 0) {
    clap_event_note_t event{};
    event.header.size = sizeof(event);
    event.header.time = time;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.header.type = type;
    event.note_id = note_id;
    event.key = key;
    event.velocity = 1.0;
    return event;
}

clap_event_note_expression_t makeExpressionEvent(uint32_t time, int32_t note_id, int16_t key,
                                                 clap_note_expression expression, double value) {
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

struct EventList {
    std::vector<const clap_event_header_t*> events;
    clap_input_events_t input{
        .ctx = this,
        .size = [](const clap_input_events_t* list) -> uint32_t {
            return static_cast<uint32_t>(static_cast<EventList*>(list->ctx)->events.size());
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

void processEvents(SimdPolySynth& synth, EventList& events) {
    synth.process(applause::BufferView<float>{}, &events.input);
}

struct SynthFixture {
    SimdPolySynth::ModMatrix matrix{{SimdPolySynth::batch_count, 8, 32, 64}};
    SimdPolySynth synth{matrix};
};

struct EnvelopeFixture {
    applause::ParamsExtension params{16};
    SimdPolySynth::ModMatrix matrix{{SimdPolySynth::batch_count, 8, 32, 64}};
    SimdPolySynth synth{matrix};

    EnvelopeFixture(float attack, float decay, float sustain, float release) {
        registerOscillatorParameters(params);
        registerEnvelopeParameters(params, attack, decay, sustain, release);
        matrix.registerFromParamsExtension(params);
        synth.bindParameters();
    }

    void process(applause::BufferView<float> output, const clap_input_events_t* events = nullptr) {
        synth.process(output, events);
    }
};
}  // namespace

TEST_CASE("SimdPolySynth uses four full SIMD batches", "[dsp][simd-poly-synth]") {
    STATIC_REQUIRE(SimdPolySynth::Batch::size == 4);
    STATIC_REQUIRE(SimdPolySynth::voice_count == 16);
    STATIC_REQUIRE(SimdPolySynth::lane_count == 4);
    STATIC_REQUIRE(SimdPolySynth::batch_count == 4);
}

TEST_CASE("SimdOscillator lifecycle masks preserve sibling lanes", "[dsp][simd-poly-synth][oscillator]") {
    SimdOscillator oscillator;
    oscillator.activate(sample_rate);

    const std::array<float, 4> frequencies{440.0f, 660.0f, 880.0f, 1100.0f};
    oscillator.start(laneMask(0b0101), SimdOscillator::Batch::load_unaligned(frequencies.data()));

    auto increments = oscillator.phaseIncrement();
    REQUIRE(increments.get(0) == Catch::Approx(440.0 / sample_rate));
    REQUIRE(increments.get(1) == 0.0f);
    REQUIRE(increments.get(2) == Catch::Approx(880.0 / sample_rate));
    REQUIRE(increments.get(3) == 0.0f);

    const auto first = oscillator.processSample<SimdOscillator::Waveform::Sine>();
    REQUIRE(first.get(0) == Catch::Approx(0.0f).margin(1.0e-7));
    REQUIRE(first.get(2) == Catch::Approx(0.0f).margin(1.0e-7));

    const auto second = oscillator.processSample<SimdOscillator::Waveform::Sine>();
    REQUIRE(second.get(0) == Catch::Approx(std::sin(two_pi * 440.0 / sample_rate)).margin(1.0e-6));
    REQUIRE(second.get(2) == Catch::Approx(std::sin(two_pi * 880.0 / sample_rate)).margin(1.0e-6));

    oscillator.kill(laneMask(0b0001));
    oscillator.start(laneMask(0b0010), SimdOscillator::Batch::load_unaligned(frequencies.data()));
    const auto continued = oscillator.processSample<SimdOscillator::Waveform::Sine>();
    REQUIRE(continued.get(0) == 0.0f);
    REQUIRE(continued.get(1) == Catch::Approx(0.0f).margin(1.0e-7));
    REQUIRE(continued.get(2) == Catch::Approx(std::sin(two_pi * 2.0 * 880.0 / sample_rate)).margin(1.0e-6));

    oscillator.reset(laneMask(0b0100));
    REQUIRE(oscillator.phase().get(2) == 0.0f);
    REQUIRE(oscillator.phaseIncrement().get(2) == Catch::Approx(880.0 / sample_rate));
    oscillator.kill(laneMask(0b0100));
    REQUIRE(oscillator.phaseIncrement().get(2) == 0.0f);
}

TEST_CASE("SimdPolySynth routes independent oscillator waveforms and controls",
          "[dsp][simd-poly-synth][oscillator][modmatrix]") {
    EnvelopeFixture fixture{0.0f, 0.0f, 1.0f, 0.0f};
    auto& params = fixture.params;
    params.getInfo("osc1_waveform").setValueSilently(2.0f);
    params.getInfo("osc1_octave").setValueSilently(-1.0f);
    params.getInfo("osc1_semitone").setValueSilently(7.0f);
    params.getInfo("osc1_fine").setValueSilently(50.0f);
    params.getInfo("osc1_level").setValueSilently(0.25f);
    params.getInfo("osc1_pan").setValueSilently(-1.0f);
    params.getInfo("osc2_waveform").setValueSilently(3.0f);
    params.getInfo("osc2_octave").setValueSilently(1.0f);
    params.getInfo("osc2_semitone").setValueSilently(-5.0f);
    params.getInfo("osc2_fine").setValueSilently(-25.0f);
    params.getInfo("osc2_level").setValueSilently(0.75f);
    params.getInfo("osc2_pan").setValueSilently(1.0f);
    REQUIRE(fixture.matrix.loadParamBaseValues(params));

    REQUIRE(fixture.matrix.findDestination("osc1_waveform")->mode == applause::ModDstMode::Mono);
    REQUIRE(fixture.matrix.findDestination("osc1_octave")->mode == applause::ModDstMode::Poly);
    REQUIRE(fixture.matrix.findDestination("osc1_semitone")->mode == applause::ModDstMode::Poly);

    fixture.synth.activate({sample_rate, 1, 4});
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    EventList events;
    events.add(note_on);

    std::array<float, 4> left{};
    std::array<float, 4> right{};
    std::array<float*, 2> channels{left.data(), right.data()};
    fixture.process({channels.data(), channels.size(), left.size()}, &events.input);

    const auto first_frequency = 440.0 * std::pow(2.0, (-12.0 + 7.0 + 0.5) / 12.0);
    const auto second_frequency = 440.0 * std::pow(2.0, (12.0 - 5.0 - 0.25) / 12.0);
    for (std::size_t frame = 0; frame < left.size(); ++frame) {
        const auto first_increment = first_frequency / sample_rate;
        const auto second_increment = second_frequency / sample_rate;
        const auto expected_left =
            0.25 * centeredSaw(static_cast<double>(frame) * first_increment, first_increment);
        const auto expected_right =
            0.75 * centeredSquare(static_cast<double>(frame) * second_increment, second_increment);
        CAPTURE(frame);
        REQUIRE(left[frame] == Catch::Approx(expected_left).margin(1.0e-5));
        REQUIRE(right[frame] == Catch::Approx(expected_right).margin(1.0e-5));
    }
}

TEST_CASE("SimdPolySynth refreshes oscillator controls without resetting phase",
          "[dsp][simd-poly-synth][oscillator][modmatrix]") {
    EnvelopeFixture fixture{0.0f, 0.0f, 1.0f, 0.0f};
    auto& params = fixture.params;
    params.getInfo("osc1_pan").setValueSilently(-1.0f);
    params.getInfo("osc2_level").setValueSilently(0.0f);
    REQUIRE(fixture.matrix.loadParamBaseValues(params));

    fixture.synth.activate({sample_rate, 1, 2});
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    EventList events;
    events.add(note_on);

    std::array<float, 2> initial_left{};
    std::array<float, 2> initial_right{};
    std::array<float*, 2> initial_channels{initial_left.data(), initial_right.data()};
    fixture.process({initial_channels.data(), initial_channels.size(), initial_left.size()}, &events.input);
    REQUIRE(initial_left[1] == Catch::Approx(std::sin(two_pi * 440.0 / sample_rate)).margin(1.0e-5));
    REQUIRE(initial_right[1] == Catch::Approx(0.0f).margin(1.0e-7));

    params.getInfo("osc1_octave").setValueSilently(1.0f);
    params.getInfo("osc1_level").setValueSilently(0.5f);
    params.getInfo("osc1_pan").setValueSilently(1.0f);
    REQUIRE(fixture.matrix.loadParamBaseValues(params));

    std::array<float, 2> updated_left{};
    std::array<float, 2> updated_right{};
    std::array<float*, 2> updated_channels{updated_left.data(), updated_right.data()};
    fixture.process({updated_channels.data(), updated_channels.size(), updated_left.size()});

    REQUIRE(updated_left[0] == Catch::Approx(0.0f).margin(1.0e-7));
    REQUIRE(updated_left[1] == Catch::Approx(0.0f).margin(1.0e-7));
    REQUIRE(updated_right[0] ==
            Catch::Approx(0.5 * std::sin(two_pi * 2.0 * 440.0 / sample_rate)).margin(1.0e-5));
    REQUIRE(updated_right[1] ==
            Catch::Approx(0.5 * std::sin(two_pi * (2.0 * 440.0 + 880.0) / sample_rate)).margin(1.0e-5));
}

TEST_CASE("SimdPolySynth changes waveform at a range boundary without resetting phase",
          "[dsp][simd-poly-synth][oscillator][modmatrix]") {
    EnvelopeFixture fixture{0.0f, 0.0f, 1.0f, 0.0f};
    auto& params = fixture.params;
    params.getInfo("osc1_pan").setValueSilently(-1.0f);
    params.getInfo("osc2_level").setValueSilently(0.0f);
    REQUIRE(fixture.matrix.loadParamBaseValues(params));

    fixture.synth.activate({sample_rate, 1, 2});
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    EventList events;
    events.add(note_on);

    std::array<float, 2> sine_left{};
    std::array<float, 2> sine_right{};
    std::array<float*, 2> sine_channels{sine_left.data(), sine_right.data()};
    fixture.process({sine_channels.data(), sine_channels.size(), sine_left.size()}, &events.input);

    params.getInfo("osc1_waveform").setValueSilently(2.0f);
    REQUIRE(fixture.matrix.loadParamBaseValues(params));

    std::array<float, 2> saw_left{};
    std::array<float, 2> saw_right{};
    std::array<float*, 2> saw_channels{saw_left.data(), saw_right.data()};
    fixture.process({saw_channels.data(), saw_channels.size(), saw_left.size()});

    const auto increment = 440.0 / sample_rate;
    REQUIRE(saw_left[0] == Catch::Approx(centeredSaw(2.0 * increment, increment)).margin(1.0e-5));
    REQUIRE(saw_left[1] == Catch::Approx(centeredSaw(3.0 * increment, increment)).margin(1.0e-5));
    REQUIRE(saw_right[0] == 0.0f);
    REQUIRE(saw_right[1] == 0.0f);
}

TEST_CASE("SimdPolySynth naively adds different oscillator waveforms",
          "[dsp][simd-poly-synth][oscillator][modmatrix]") {
    EnvelopeFixture fixture{0.0f, 0.0f, 1.0f, 0.0f};
    auto& params = fixture.params;
    params.getInfo("osc1_waveform").setValueSilently(2.0f);
    params.getInfo("osc2_waveform").setValueSilently(3.0f);
    REQUIRE(fixture.matrix.loadParamBaseValues(params));

    fixture.synth.activate({sample_rate, 1, 4});
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    EventList events;
    events.add(note_on);

    std::array<float, 4> left{};
    std::array<float, 4> right{};
    std::array<float*, 2> channels{left.data(), right.data()};
    fixture.process({channels.data(), channels.size(), left.size()}, &events.input);

    const auto increment = 440.0 / sample_rate;
    for (std::size_t frame = 0; frame < left.size(); ++frame) {
        const auto phase = static_cast<double>(frame) * increment;
        const auto expected = (centeredSaw(phase, increment) + centeredSquare(phase, increment)) *
            0.70710678118654752440;
        CAPTURE(frame);
        REQUIRE(left[frame] == Catch::Approx(expected).margin(1.0e-5));
        REQUIRE(right[frame] == Catch::Approx(expected).margin(1.0e-5));
    }
}

TEST_CASE("SimdPolySynth routes its envelope into oscillator level at range boundaries",
          "[dsp][simd-poly-synth][oscillator][modmatrix]") {
    constexpr std::size_t attack_samples = 64;
    EnvelopeFixture fixture{static_cast<float>(attack_samples / sample_rate), 0.0f, 1.0f, 0.0f};
    fixture.params.getInfo("osc1_level").setValueSilently(0.0f);
    fixture.params.getInfo("osc2_level").setValueSilently(0.0f);
    REQUIRE(fixture.matrix.loadParamBaseValues(fixture.params));
    fixture.matrix.addConnection(*fixture.matrix.findSource("Envelope"), *fixture.matrix.findDestination("osc1_level"),
                                 1.0f, false);

    fixture.synth.activate({sample_rate, 1, 17});
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    EventList events;
    events.add(note_on);

    std::array<float, 17> silent_left{};
    std::array<float, 17> silent_right{};
    std::array<float*, 2> silent_channels{silent_left.data(), silent_right.data()};
    fixture.process({silent_channels.data(), silent_channels.size(), silent_left.size()}, &events.input);
    REQUIRE(std::ranges::all_of(silent_left, [](float sample) { return sample == 0.0f; }));
    REQUIRE(std::ranges::all_of(silent_right, [](float sample) { return sample == 0.0f; }));

    std::array<float, 1> routed_left{};
    std::array<float, 1> routed_right{};
    std::array<float*, 2> routed_channels{routed_left.data(), routed_right.data()};
    fixture.process({routed_channels.data(), routed_channels.size(), routed_left.size()});

    const auto expected = std::sin(two_pi * 440.0 * 17.0 / sample_rate) * envelopeProgress(17, attack_samples) *
        envelopeProgress(18, attack_samples) * 0.70710678118654752440;
    REQUIRE(routed_left[0] == Catch::Approx(expected).margin(1.0e-5));
    REQUIRE(routed_right[0] == Catch::Approx(expected).margin(1.0e-5));
}

TEST_CASE("SimdPolySynth tuning expressions retune both oscillators without resetting phase",
          "[dsp][simd-poly-synth][oscillator][expression]") {
    EnvelopeFixture fixture{0.0f, 0.0f, 1.0f, 0.0f};
    fixture.params.getInfo("osc1_pan").setValueSilently(-1.0f);
    fixture.params.getInfo("osc2_semitone").setValueSilently(12.0f);
    fixture.params.getInfo("osc2_pan").setValueSilently(1.0f);
    REQUIRE(fixture.matrix.loadParamBaseValues(fixture.params));
    fixture.synth.activate({sample_rate, 1, 2});

    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    EventList note_events;
    note_events.add(note_on);
    std::array<float, 2> initial_left{};
    std::array<float, 2> initial_right{};
    std::array<float*, 2> initial_channels{initial_left.data(), initial_right.data()};
    fixture.process({initial_channels.data(), initial_channels.size(), initial_left.size()}, &note_events.input);

    auto tuning = makeExpressionEvent(0, 1, 69, CLAP_NOTE_EXPRESSION_TUNING, 12.0);
    EventList expression_events;
    expression_events.add(tuning);
    std::array<float, 2> tuned_left{};
    std::array<float, 2> tuned_right{};
    std::array<float*, 2> tuned_channels{tuned_left.data(), tuned_right.data()};
    fixture.process({tuned_channels.data(), tuned_channels.size(), tuned_left.size()}, &expression_events.input);

    REQUIRE(tuned_left[0] ==
            Catch::Approx(std::sin(two_pi * 2.0 * 440.0 / sample_rate)).margin(1.0e-5));
    REQUIRE(tuned_left[1] ==
            Catch::Approx(std::sin(two_pi * (2.0 * 440.0 + 880.0) / sample_rate)).margin(1.0e-5));
    REQUIRE(tuned_right[0] ==
            Catch::Approx(std::sin(two_pi * 2.0 * 880.0 / sample_rate)).margin(1.0e-5));
    REQUIRE(tuned_right[1] ==
            Catch::Approx(std::sin(two_pi * (2.0 * 880.0 + 1760.0) / sample_rate)).margin(1.0e-5));
}

TEST_CASE("SimdPolySynth uses the plugin-owned parameter modulation matrix", "[dsp][simd-poly-synth][modmatrix]") {
    applause::ParamsExtension params{2};
    params.registerParam({.string_id = "filter_cutoff",
                          .name = "Filter Cutoff",
                          .module = "Filter",
                          .short_name = "Cutoff",
                          .unit = "Hz",
                          .min_value = 20.0f,
                          .max_value = 20000.0f,
                          .default_value = 440.0f,
                          .is_polyphonic = true,
                          .scaling = applause::ValueScaling::frequency(20.0f, 20000.0f)});
    params.registerParam({.string_id = "output_level",
                          .name = "Output Level",
                          .module = "Output",
                          .short_name = "Level",
                          .min_value = 0.0f,
                          .max_value = 4.0f,
                          .default_value = 1.0f,
                          .scaling = applause::ValueScaling::quadratic()});

    SynthFixture fixture;
    auto& synth = fixture.synth;
    auto& matrix = fixture.matrix;

    REQUIRE(matrix.getSourceCount() == 3);
    REQUIRE(matrix.getDestinationCount() == 0);
    REQUIRE(matrix.getConnections().empty());

    const auto* envelope = matrix.findSource("Envelope");
    const auto* timbre = matrix.findSource("Timbre");
    const auto* pressure = matrix.findSource("Pressure");
    REQUIRE(envelope != nullptr);
    REQUIRE(timbre != nullptr);
    REQUIRE(pressure != nullptr);
    REQUIRE(envelope->type == applause::ModSrcType::Poly);
    REQUIRE(timbre->type == applause::ModSrcType::Poly);
    REQUIRE(pressure->type == applause::ModSrcType::Poly);
    REQUIRE_FALSE(timbre->bipolar);
    REQUIRE_FALSE(pressure->bipolar);

    matrix.registerFromParamsExtension(params);

    REQUIRE(matrix.getSourceCount() == 3);
    REQUIRE(matrix.getDestinationCount() == 2);
    REQUIRE(matrix.getConnections().empty());

    const auto* cutoff = matrix.findDestination("filter_cutoff");
    const auto* level = matrix.findDestination("output_level");
    REQUIRE(cutoff != nullptr);
    REQUIRE(level != nullptr);
    REQUIRE(cutoff->index == 0);
    REQUIRE(level->index == 1);
    REQUIRE(cutoff->name == "Filter / Cutoff");
    REQUIRE(level->name == "Output / Level");
    REQUIRE(cutoff->mode == applause::ModDstMode::Poly);
    REQUIRE(level->mode == applause::ModDstMode::Mono);

    synth.activate({sample_rate, 1, 1});
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    EventList note_events;
    note_events.add(note_on);

    std::array<float, 1> left{};
    std::array<float, 1> right{};
    std::array<float*, 2> channels{left.data(), right.data()};
    synth.process({channels.data(), channels.size(), left.size()}, &note_events.input);

    std::array<float, 1> cutoff_values{};
    REQUIRE(matrix.copyActiveDestinationValues(cutoff->index, cutoff_values) == 1);
    REQUIRE(cutoff_values[0] == Catch::Approx(440.0f).margin(0.001f));
    REQUIRE(matrix.getModValue(level->index) == Catch::Approx(1.0f));

    params.getInfo("filter_cutoff").setValueSilently(1760.0f);
    params.getInfo("output_level").setValueSilently(3.24f);
    REQUIRE(matrix.loadParamBaseValues(params));

    synth.reset();
    note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 2, 81);
    synth.process({channels.data(), channels.size(), left.size()}, &note_events.input);

    REQUIRE(matrix.copyActiveDestinationValues(cutoff->index, cutoff_values) == 1);
    REQUIRE(cutoff_values[0] == Catch::Approx(1760.0f).margin(0.01f));
    REQUIRE(matrix.getModValue(level->index) == Catch::Approx(3.24f).margin(0.001f));
}

TEST_CASE("SimdPolySynth exposes CLAP timbre and pressure as polyphonic modulation sources",
          "[dsp][simd-poly-synth][modmatrix][expression]") {
    applause::ParamsExtension params{2};
    params.registerParam({.string_id = "timbre_target",
                          .name = "Timbre Target",
                          .min_value = 0.0f,
                          .max_value = 1.0f,
                          .default_value = 0.0f,
                          .is_polyphonic = true});
    params.registerParam({.string_id = "pressure_target",
                          .name = "Pressure Target",
                          .min_value = 0.0f,
                          .max_value = 1.0f,
                          .default_value = 0.0f,
                          .is_polyphonic = true});

    SynthFixture fixture;
    auto& matrix = fixture.matrix;
    auto& synth = fixture.synth;
    matrix.registerFromParamsExtension(params);
    const auto* timbre_target = matrix.findDestination("timbre_target");
    const auto* pressure_target = matrix.findDestination("pressure_target");
    matrix.addConnection(*matrix.findSource("Timbre"), *timbre_target, 1.0f, false);
    matrix.addConnection(*matrix.findSource("Pressure"), *pressure_target, 1.0f, false);

    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 60);
    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 2, 64);
    EventList note_events;
    note_events.add(first_note);
    note_events.add(second_note);
    processEvents(synth, note_events);

    std::array<float, 2> values{};
    REQUIRE(matrix.copyActiveDestinationValues(timbre_target->index, values) == 2);
    REQUIRE(values[0] == Catch::Approx(0.5f));
    REQUIRE(values[1] == Catch::Approx(0.5f));
    REQUIRE(matrix.copyActiveDestinationValues(pressure_target->index, values) == 2);
    REQUIRE(values[0] == 0.0f);
    REQUIRE(values[1] == 0.0f);

    auto timbre = makeExpressionEvent(0, 1, 60, CLAP_NOTE_EXPRESSION_BRIGHTNESS, 0.8);
    auto pressure = makeExpressionEvent(0, 2, 64, CLAP_NOTE_EXPRESSION_PRESSURE, 0.7);
    EventList expression_events;
    expression_events.add(timbre);
    expression_events.add(pressure);
    processEvents(synth, expression_events);

    REQUIRE(matrix.copyActiveDestinationValues(timbre_target->index, values) == 2);
    REQUIRE(values[0] == Catch::Approx(0.8f));
    REQUIRE(values[1] == Catch::Approx(0.5f));
    REQUIRE(matrix.copyActiveDestinationValues(pressure_target->index, values) == 2);
    REQUIRE(values[0] == 0.0f);
    REQUIRE(values[1] == Catch::Approx(0.7f));

    auto choke = makeNoteEvent(CLAP_EVENT_NOTE_CHOKE, 1, 60);
    auto replacement = makeNoteEvent(CLAP_EVENT_NOTE_ON, 3, 67);
    EventList replacement_events;
    replacement_events.add(choke);
    replacement_events.add(replacement);
    processEvents(synth, replacement_events);

    REQUIRE(matrix.copyActiveDestinationValues(timbre_target->index, values) == 2);
    REQUIRE(values[0] == Catch::Approx(0.5f));
    REQUIRE(values[1] == Catch::Approx(0.5f));
    REQUIRE(matrix.copyActiveDestinationValues(pressure_target->index, values) == 2);
    REQUIRE(values[0] == 0.0f);
    REQUIRE(values[1] == Catch::Approx(0.7f));
}

TEST_CASE("SimdPolySynth applies same-sample pressure before starting the note envelope",
          "[dsp][simd-poly-synth][modmatrix][expression]") {
    constexpr std::size_t attack_samples = 64;
    EnvelopeFixture fixture{0.0f, 0.0f, 1.0f, 0.0f};
    fixture.matrix.addConnection(*fixture.matrix.findSource("Pressure"), *fixture.matrix.findDestination("attack"),
                                 1.0f, false);
    fixture.synth.activate({sample_rate, 1, 2});

    auto pressure = makeExpressionEvent(0, 1, 69, CLAP_NOTE_EXPRESSION_PRESSURE,
                                        static_cast<double>(attack_samples) / sample_rate);
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    EventList events;
    events.add(pressure);
    events.add(note_on);

    std::array<float, 2> left{};
    std::array<float, 2> right{};
    std::array<float*, 2> channels{left.data(), right.data()};
    fixture.process({channels.data(), channels.size(), left.size()}, &events.input);

    const auto expected =
        std::sin(two_pi * 440.0 / sample_rate) * envelopeProgress(2, attack_samples) * voice_gain;
    REQUIRE(left[1] == Catch::Approx(expected).margin(1.0e-5));
    REQUIRE(right[1] == Catch::Approx(expected).margin(1.0e-5));
}

TEST_CASE("SimdPolySynth updates modulation before each DSP range", "[dsp][simd-poly-synth][modmatrix]") {
    applause::ParamsExtension params{17};
    params.registerParam({.string_id = "target",
                          .name = "Target",
                          .min_value = 0.0f,
                          .max_value = 1.0f,
                          .default_value = 0.0f,
                          .is_polyphonic = true});
    registerOscillatorParameters(params);
    registerEnvelopeParameters(params, 64.0f / static_cast<float>(sample_rate), 0.0f, 1.0f, 0.0f);

    SynthFixture fixture;
    auto& matrix = fixture.matrix;
    auto& synth = fixture.synth;
    matrix.registerFromParamsExtension(params);
    synth.bindParameters();
    matrix.addConnection(*matrix.findSource("Envelope"), *matrix.findDestination("target"), 1.0f, false);

    synth.activate({sample_rate, 1, 64});
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    EventList note_events;
    note_events.add(note_on);

    const auto render = [&synth](std::size_t frames, const clap_input_events_t* events = nullptr) {
        std::vector<float> left(frames);
        std::vector<float> right(frames);
        std::array<float*, 2> channels{left.data(), right.data()};
        synth.process({channels.data(), channels.size(), frames}, events);
    };
    const auto targetValue = [&matrix] {
        std::array<float, 1> value{};
        REQUIRE(matrix.copyActiveDestinationValues(0, value) == 1);
        return value[0];
    };

    render(17, &note_events.input);
    REQUIRE(targetValue() == Catch::Approx(0.0f));
    render(15);
    REQUIRE(targetValue() == Catch::Approx(envelopeProgress(17, 64)));
    render(32);
    REQUIRE(targetValue() == Catch::Approx(envelopeProgress(32, 64)));
    render(1);
    REQUIRE(targetValue() == Catch::Approx(1.0f));
}

TEST_CASE("SimdPolySynth voices read routed envelope parameters from their SIMD lanes",
          "[dsp][simd-poly-synth][modmatrix]") {
    applause::ParamsExtension params{16};
    registerOscillatorParameters(params);
    registerEnvelopeParameters(params, 0.0f, 0.0f, 1.0f, 0.0f);

    SynthFixture fixture;
    auto& matrix = fixture.matrix;
    auto& synth = fixture.synth;
    auto& attack_source = matrix.registerSource("Attack Mod", applause::ModSrcType::Poly, false);
    matrix.registerFromParamsExtension(params);
    synth.bindParameters();
    matrix.addConnection(attack_source, *matrix.findDestination("attack"), 1.0f, false);

    constexpr std::size_t first_attack_samples = 64;
    constexpr std::size_t second_attack_samples = 128;
    const std::array<float, SimdPolySynth::lane_count> attack_values{
        static_cast<float>(first_attack_samples / sample_rate), static_cast<float>(second_attack_samples / sample_rate),
        0.0f, 0.0f};
    matrix.setPolySourceValue(attack_source.index, 0, SimdPolySynth::Batch::load_unaligned(attack_values.data()));

    synth.activate({sample_rate, 1, 2});
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 2, 69);
    EventList events;
    events.add(first_note);
    events.add(second_note);

    std::array<float, 2> left{};
    std::array<float, 2> right{};
    std::array<float*, 2> channels{left.data(), right.data()};
    synth.process({channels.data(), channels.size(), left.size()}, &events.input);

    const auto expected = std::sin(two_pi * 440.0 / sample_rate) *
        (envelopeProgress(2, first_attack_samples) + envelopeProgress(2, second_attack_samples)) * voice_gain;
    REQUIRE(left[1] == Catch::Approx(expected).margin(1.0e-5));
    REQUIRE(right == left);
}

TEST_CASE("SimdPolySynth snapshots routed release at note-off", "[dsp][simd-poly-synth][modmatrix]") {
    using Voice = applause::SynthesizerVoice<float>;

    applause::ParamsExtension params{16};
    registerOscillatorParameters(params);
    registerEnvelopeParameters(params, 0.0f, 0.0f, 1.0f, 0.0f);

    SynthFixture fixture;
    auto& matrix = fixture.matrix;
    auto& synth = fixture.synth;
    auto& release_source = matrix.registerSource("Release Mod", applause::ModSrcType::Poly, false);
    matrix.registerFromParamsExtension(params);
    synth.bindParameters();
    matrix.addConnection(release_source, *matrix.findDestination("release"), 1.0f, false);
    synth.activate({sample_rate, 1, 3});

    matrix.setPolySourceValue(release_source.index, 0, SimdPolySynth::Batch{8.0f / static_cast<float>(sample_rate)});
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    EventList note_on_events;
    note_on_events.add(note_on);

    std::array<float, 1> held_left{};
    std::array<float, 1> held_right{};
    std::array<float*, 2> held_channels{held_left.data(), held_right.data()};
    synth.process({held_channels.data(), held_channels.size(), held_left.size()}, &note_on_events.input);

    matrix.setPolySourceValue(release_source.index, 0, SimdPolySynth::Batch{4.0f / static_cast<float>(sample_rate)});
    auto note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 1, 69);
    EventList note_off_events;
    note_off_events.add(note_off);

    std::array<float, 3> release_left{};
    std::array<float, 3> release_right{};
    std::array<float*, 2> release_channels{release_left.data(), release_right.data()};
    synth.process({release_channels.data(), release_channels.size(), release_left.size()}, &note_off_events.input);
    REQUIRE(synth.getVoices()[0].active_);
    REQUIRE(synth.getVoices()[0].state_ == Voice::State::Released);

    matrix.setPolySourceValue(release_source.index, 0, SimdPolySynth::Batch{8.0f / static_cast<float>(sample_rate)});
    std::array<float, 1> final_left{};
    std::array<float, 1> final_right{};
    std::array<float*, 2> final_channels{final_left.data(), final_right.data()};
    synth.process({final_channels.data(), final_channels.size(), final_left.size()}, nullptr);

    REQUIRE(final_left[0] == 0.0f);
    REQUIRE(final_right[0] == 0.0f);
    REQUIRE_FALSE(synth.getVoices()[0].active_);
    REQUIRE(synth.getVoices()[0].state_ == Voice::State::Idle);
}

TEST_CASE("BatchedVoice tracks occupied lanes", "[dsp][simd-poly-synth]") {
    BatchedVoice batch;
    applause::Note first_note;
    first_note.key = 69;
    first_note.note_on_velocity = 1.0;
    applause::Note second_note = first_note;
    second_note.key = 81;
    REQUIRE(batch.empty());
    REQUIRE(batch.occupiedMask().mask() == 0);

    batch.kill(laneMask(0b0100));
    REQUIRE(batch.empty());

    batch.start(laneMask(0b0001), first_note);
    batch.start(laneMask(0b1000), second_note);
    REQUIRE(batch.occupiedMask().mask() == 0b1001);
    REQUIRE(batch.isOccupied(laneMask(0b0001)));
    REQUIRE_FALSE(batch.isOccupied(laneMask(0b0010)));
    REQUIRE(batch.isOccupied(laneMask(0b1000)));

    batch.kill(laneMask(0b0001));
    REQUIRE(batch.occupiedMask().mask() == 0b1000);
    REQUIRE(batch.isOccupied(laneMask(0b1000)));

    batch.kill(laneMask(0b1000));
    REQUIRE(batch.empty());
}

TEST_CASE("BatchedVoice stages and clears expression source lanes", "[dsp][simd-poly-synth][expression]") {
    BatchedVoice batch;
    applause::Note first;
    first.brightness = 0.2;
    first.pressure = 0.3;
    applause::Note second;
    second.brightness = 0.8;
    second.pressure = 0.9;

    batch.preProcess(laneMask(0b0010), first);
    batch.preProcess(laneMask(0b1000), second);
    REQUIRE(batch.timbreValue().get(1) == Catch::Approx(0.2f));
    REQUIRE(batch.pressureValue().get(1) == Catch::Approx(0.3f));
    REQUIRE(batch.timbreValue().get(3) == Catch::Approx(0.8f));
    REQUIRE(batch.pressureValue().get(3) == Catch::Approx(0.9f));

    batch.kill(laneMask(0b0010));
    REQUIRE(batch.timbreValue().get(1) == 0.0f);
    REQUIRE(batch.pressureValue().get(1) == 0.0f);
    REQUIRE(batch.timbreValue().get(3) == Catch::Approx(0.8f));
    REQUIRE(batch.pressureValue().get(3) == Catch::Approx(0.9f));

    batch.preProcess(laneMask(0b0010), applause::Note{});
    REQUIRE(batch.timbreValue().get(1) == Catch::Approx(0.5f));
    REQUIRE(batch.pressureValue().get(1) == 0.0f);
}

TEST_CASE("SimdVoiceManager routes slots to fixed batches", "[dsp][simd-poly-synth]") {
    std::array<SimdVoiceProxy, SimdVoiceManager::voice_count> proxies;
    SimdVoiceManager manager;
    applause::Note note;
    note.key = 69;
    manager.bind(proxies);
    REQUIRE(manager.empty());

    manager.start(0, note);
    manager.start(3, note);
    manager.start(4, note);
    manager.start(15, note);

    REQUIRE(manager.batches()[0].occupiedMask().mask() == 0b1001);
    REQUIRE(manager.batches()[1].occupiedMask().mask() == 0b0001);
    REQUIRE(manager.batches()[2].occupiedMask().mask() == 0);
    REQUIRE(manager.batches()[3].occupiedMask().mask() == 0b1000);
}

TEST_CASE("SimdPolySynth proxy types preserve stable bindings", "[dsp][simd-poly-synth]") {
    STATIC_REQUIRE(std::is_base_of_v<applause::SynthesizerVoice<float>, SimdVoiceProxy>);
    STATIC_REQUIRE(std::is_default_constructible_v<SimdVoiceProxy>);
    STATIC_REQUIRE(!std::is_copy_constructible_v<SimdVoiceProxy>);
    STATIC_REQUIRE(!std::is_move_constructible_v<SimdVoiceProxy>);
    STATIC_REQUIRE(!std::is_copy_assignable_v<SimdVoiceProxy>);
    STATIC_REQUIRE(!std::is_move_assignable_v<SimdVoiceProxy>);
    STATIC_REQUIRE(!std::is_copy_constructible_v<SimdVoiceManager>);
    STATIC_REQUIRE(!std::is_move_constructible_v<SimdVoiceManager>);
    STATIC_REQUIRE(!std::is_copy_assignable_v<SimdVoiceManager>);
    STATIC_REQUIRE(!std::is_move_assignable_v<SimdVoiceManager>);
    STATIC_REQUIRE(!std::is_copy_constructible_v<SimdPolySynth>);
    STATIC_REQUIRE(!std::is_move_constructible_v<SimdPolySynth>);
    STATIC_REQUIRE(!std::is_copy_assignable_v<SimdPolySynth>);
    STATIC_REQUIRE(!std::is_move_assignable_v<SimdPolySynth>);
    SynthFixture fixture;
    auto& synth = fixture.synth;
    const auto voices = synth.getVoices();
    REQUIRE(voices.size() == SimdPolySynth::voice_count);

    for (std::size_t slot = 0; slot < voices.size(); ++slot) {
        REQUIRE(voices[slot].isBound());
        REQUIRE(voices[slot].logicalSlot() == slot);
    }
}

TEST_CASE("SimdPolySynth stops notes and chokes immediately", "[dsp][simd-poly-synth]") {
    using Voice = applause::SynthesizerVoice<float>;

    SynthFixture fixture;
    auto& synth = fixture.synth;
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 60);
    auto second_note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 2, 64);
    auto note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 1, 60);
    auto second_note_choke = makeNoteEvent(CLAP_EVENT_NOTE_CHOKE, 2, 64);

    EventList note_on_events;
    note_on_events.add(note_on);
    note_on_events.add(second_note_on);
    processEvents(synth, note_on_events);
    REQUIRE_FALSE(synth.empty());
    REQUIRE(synth.getVoices()[0].state_ == Voice::State::KeyDown);

    EventList note_off_events;
    note_off_events.add(note_off);
    processEvents(synth, note_off_events);
    REQUIRE_FALSE(synth.empty());
    REQUIRE_FALSE(synth.getVoices()[0].active_);
    REQUIRE(synth.getVoices()[0].state_ == Voice::State::Idle);
    REQUIRE(synth.getVoices()[1].active_);

    EventList choke_events;
    choke_events.add(second_note_choke);
    processEvents(synth, choke_events);
    REQUIRE(synth.empty());
    REQUIRE_FALSE(synth.getVoices()[1].active_);
}

TEST_CASE("SimdPolySynth chokes a deferred same-sample note start",
          "[dsp][simd-poly-synth]") {
    SynthFixture fixture;
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 60);
    auto choke = makeNoteEvent(CLAP_EVENT_NOTE_CHOKE, 1, 60);
    EventList events;
    events.add(note_on);
    events.add(choke);

    processEvents(fixture.synth, events);

    REQUIRE(fixture.synth.empty());
    REQUIRE_FALSE(fixture.synth.getVoices()[0].active_);
}

TEST_CASE("SimdPolySynth reuses a stolen SIMD lane", "[dsp][simd-poly-synth]") {
    SynthFixture fixture;
    auto& synth = fixture.synth;
    synth.activate({sample_rate, 1, 64});

    std::array<clap_event_note_t, SimdPolySynth::voice_count> note_ons{};
    EventList note_on_events;
    for (std::size_t slot = 0; slot < SimdPolySynth::voice_count; ++slot) {
        note_ons[slot] =
            makeNoteEvent(CLAP_EVENT_NOTE_ON, static_cast<int32_t>(slot + 1), static_cast<int16_t>(48 + slot));
        note_on_events.add(note_ons[slot]);
    }
    processEvents(synth, note_on_events);

    auto stolen_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 17, 72);
    EventList stolen_note_events;
    stolen_note_events.add(stolen_note);
    processEvents(synth, stolen_note_events);

    const auto voices = synth.getVoices();
    REQUIRE(voices[0].active_);
    REQUIRE(voices[0].note_.note_id == 17);
    for (std::size_t slot = 1; slot < voices.size(); ++slot) {
        REQUIRE(voices[slot].active_);
        REQUIRE(voices[slot].note_.note_id == static_cast<int32_t>(slot + 1));
    }
}

TEST_CASE("SimdPolySynth reset clears held lanes", "[dsp][simd-poly-synth]") {
    using Voice = applause::SynthesizerVoice<float>;

    SynthFixture fixture;
    auto& synth = fixture.synth;
    auto first_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 60);
    auto second_note = makeNoteEvent(CLAP_EVENT_NOTE_ON, 2, 64);
    EventList note_events;
    note_events.add(first_note);
    note_events.add(second_note);
    processEvents(synth, note_events);

    synth.reset();
    REQUIRE(synth.empty());
    for (const auto& voice : synth.getVoices()) {
        REQUIRE_FALSE(voice.active_);
        REQUIRE(voice.state_ == Voice::State::Idle);
    }

    synth.reset();
    REQUIRE(synth.empty());
}

TEST_CASE("SimdPolySynth renders five sine voices across two batches", "[dsp][simd-poly-synth]") {
    SynthFixture fixture;
    auto& synth = fixture.synth;
    synth.activate({sample_rate, 1, 16});

    std::array<clap_event_note_t, 5> note_ons{};
    EventList note_events;
    for (int32_t note_id = 1; note_id <= 5; ++note_id) {
        const int16_t key = note_id % 2 == 0 ? 81 : 69;
        note_ons[static_cast<std::size_t>(note_id - 1)] = makeNoteEvent(CLAP_EVENT_NOTE_ON, note_id, key);
        note_events.add(note_ons[static_cast<std::size_t>(note_id - 1)]);
    }

    std::array<float, 16> first_left{};
    std::array<float, 16> first_right{};
    std::array<float*, 2> first_channels{first_left.data(), first_right.data()};
    applause::BufferView<float> first_output{first_channels.data(), first_channels.size(), first_left.size()};
    synth.process(first_output, &note_events.input);

    std::array<float, 16> second_left{};
    std::array<float, 16> second_right{};
    std::array<float*, 2> second_channels{second_left.data(), second_right.data()};
    applause::BufferView<float> second_output{second_channels.data(), second_channels.size(), second_left.size()};
    synth.process(second_output, nullptr);

    const auto expectedAt = [](std::size_t frame) {
        const auto time = static_cast<double>(frame) / sample_rate;
        return (3.0 * std::sin(two_pi * 440.0 * time) + 2.0 * std::sin(two_pi * 880.0 * time)) * voice_gain;
    };
    for (std::size_t frame = 0; frame < first_left.size(); ++frame) {
        const auto first_expected = expectedAt(frame);
        const auto second_expected = expectedAt(frame + 16);
        REQUIRE(first_left[frame] == Catch::Approx(first_expected).margin(1.0e-5));
        REQUIRE(first_right[frame] == Catch::Approx(first_expected).margin(1.0e-5));
        REQUIRE(second_left[frame] == Catch::Approx(second_expected).margin(1.0e-5));
        REQUIRE(second_right[frame] == Catch::Approx(second_expected).margin(1.0e-5));
    }
}

TEST_CASE("SimdPolySynth isolates lanes at exact event offsets", "[dsp][simd-poly-synth]") {
    SynthFixture fixture;
    auto& synth = fixture.synth;
    synth.activate({sample_rate, 1, 20});

    auto first_note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69, 4);
    auto second_note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 2, 81, 4);
    auto first_note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 1, 69, 12);
    auto second_note_choke = makeNoteEvent(CLAP_EVENT_NOTE_CHOKE, 2, 81, 18);
    EventList events;
    events.add(first_note_on);
    events.add(second_note_on);
    events.add(first_note_off);
    events.add(second_note_choke);

    std::array<float, 20> left;
    std::array<float, 20> right;
    left.fill(1.0f);
    right.fill(1.0f);
    std::array<float*, 2> channels{left.data(), right.data()};
    applause::BufferView<float> output{channels.data(), channels.size(), left.size()};
    synth.process(output, &events.input);

    for (std::size_t frame = 0; frame < left.size(); ++frame) {
        double expected = 0.0;
        if (frame >= 4 && frame < 18) {
            const auto time = static_cast<double>(frame - 4) / sample_rate;
            expected = std::sin(two_pi * 880.0 * time) * voice_gain;
            if (frame < 12) expected += std::sin(two_pi * 440.0 * time) * voice_gain;
        }
        REQUIRE(left[frame] == Catch::Approx(expected).margin(1.0e-5));
        REQUIRE(right[frame] == Catch::Approx(expected).margin(1.0e-5));
    }
    REQUIRE(synth.empty());
}

TEST_CASE("SimdPolySynth applies its ADSR per sample across process partitions", "[dsp][simd-poly-synth]") {
    const auto configure = [](EnvelopeFixture& fixture) {
        fixture.synth.activate({sample_rate, 1, 96});
        auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
        EventList note_events;
        note_events.add(note_on);
        processEvents(fixture.synth, note_events);
    };

    EnvelopeFixture contiguous_fixture{64.0f / static_cast<float>(sample_rate), 0.0f, 1.0f, 0.0f};
    configure(contiguous_fixture);
    std::array<float, 96> reference_left{};
    std::array<float, 96> reference_right{};
    std::array<float*, 2> reference_channels{reference_left.data(), reference_right.data()};
    contiguous_fixture.process({reference_channels.data(), reference_channels.size(), reference_left.size()});

    EnvelopeFixture partitioned_fixture{64.0f / static_cast<float>(sample_rate), 0.0f, 1.0f, 0.0f};
    configure(partitioned_fixture);
    std::array<float, 96> partitioned_left{};
    std::array<float, 96> partitioned_right{};
    constexpr std::array<std::size_t, 3> block_sizes{17, 23, 56};
    std::size_t offset = 0;
    for (const auto block_size : block_sizes) {
        std::array<float*, 2> channels{partitioned_left.data() + offset, partitioned_right.data() + offset};
        partitioned_fixture.process({channels.data(), channels.size(), block_size});
        offset += block_size;
    }

    for (std::size_t frame = 0; frame < reference_left.size(); ++frame) {
        const auto envelope = envelopeProgress(std::min(frame + 1, std::size_t{64}), 64);
        const double expected =
            std::sin(two_pi * 440.0 * static_cast<double>(frame) / sample_rate) * envelope * voice_gain;
        CAPTURE(frame);
        REQUIRE(reference_left[frame] == Catch::Approx(expected).margin(1.0e-5));
        REQUIRE(reference_right[frame] == Catch::Approx(expected).margin(1.0e-5));
        REQUIRE(partitioned_left[frame] == Catch::Approx(expected).margin(1.0e-5));
        REQUIRE(partitioned_right[frame] == Catch::Approx(expected).margin(1.0e-5));
    }
}

TEST_CASE("SimdPolySynth keeps a released lane until its ADSR finishes", "[dsp][simd-poly-synth]") {
    using Voice = applause::SynthesizerVoice<float>;

    EnvelopeFixture fixture{0.0f, 0.0f, 1.0f, 64.0f / static_cast<float>(sample_rate)};
    auto& synth = fixture.synth;
    synth.activate({sample_rate, 1, 64});
    auto note_on = makeNoteEvent(CLAP_EVENT_NOTE_ON, 1, 69);
    auto note_off = makeNoteEvent(CLAP_EVENT_NOTE_OFF, 1, 69);
    EventList note_events;
    note_events.add(note_on);
    note_events.add(note_off);
    processEvents(synth, note_events);

    REQUIRE(synth.getVoices()[0].active_);
    REQUIRE(synth.getVoices()[0].state_ == Voice::State::Released);

    std::array<float, 63> release_left{};
    std::array<float, 63> release_right{};
    std::array<float*, 2> release_channels{release_left.data(), release_right.data()};
    fixture.process({release_channels.data(), release_channels.size(), release_left.size()});
    REQUIRE(release_left[1] ==
            Catch::Approx(std::sin(two_pi * 440.0 / sample_rate) * (1.0 - envelopeProgress(2, 64)) * voice_gain)
                .margin(1.0e-5));
    REQUIRE(release_left[32] ==
            Catch::Approx(std::sin(two_pi * 440.0 * 32.0 / sample_rate) * (1.0 - envelopeProgress(33, 64)) * voice_gain)
                .margin(1.0e-5));
    REQUIRE(release_left[62] ==
            Catch::Approx(std::sin(two_pi * 440.0 * 62.0 / sample_rate) * (1.0 - envelopeProgress(63, 64)) * voice_gain)
                .margin(1.0e-5));
    REQUIRE(release_right == release_left);
    REQUIRE(synth.getVoices()[0].active_);
    REQUIRE_FALSE(synth.empty());

    std::array<float, 1> final_left{};
    std::array<float, 1> final_right{};
    std::array<float*, 2> final_channels{final_left.data(), final_right.data()};
    fixture.process({final_channels.data(), final_channels.size(), 1});
    REQUIRE_FALSE(synth.getVoices()[0].active_);
    REQUIRE(synth.getVoices()[0].state_ == Voice::State::Idle);
    REQUIRE(synth.empty());
    REQUIRE(final_left[0] == 0.0f);
    REQUIRE(final_right[0] == 0.0f);
}
