#include <catch2/catch_test_macros.hpp>

#include <applause/core/PluginBase.h>
#include <applause/core/ProcessContext.h>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

using namespace applause;

namespace {

constexpr std::size_t kFrames = 4;

struct ProcessFixture {
    std::array<float, kFrames> input_left{1.0f, 2.0f, 3.0f, 4.0f};
    std::array<float, kFrames> input_right{5.0f, 6.0f, 7.0f, 8.0f};
    std::array<double, kFrames> input_double{9.0, 10.0, 11.0, 12.0};
    std::array<float, kFrames> output_left{};
    std::array<float, kFrames> output_right{};
    std::array<double, kFrames> output_double{};

    std::array<float*, 2> input_float_channels{input_left.data(),
                                                input_right.data()};
    std::array<double*, 1> input_double_channels{input_double.data()};
    std::array<float*, 2> output_float_channels{output_left.data(),
                                                 output_right.data()};
    std::array<double*, 1> output_double_channels{output_double.data()};

    std::array<clap_audio_buffer_t, 2> inputs{
        clap_audio_buffer_t{
            .data32 = input_float_channels.data(),
            .data64 = nullptr,
            .channel_count = 2,
            .latency = 11,
            .constant_mask = 0b01,
        },
        clap_audio_buffer_t{
            .data32 = nullptr,
            .data64 = input_double_channels.data(),
            .channel_count = 1,
            .latency = 12,
            .constant_mask = 0b1,
        },
    };

    std::array<clap_audio_buffer_t, 2> outputs{
        clap_audio_buffer_t{
            .data32 = output_float_channels.data(),
            .data64 = nullptr,
            .channel_count = 2,
            .latency = 21,
            .constant_mask = 0,
        },
        clap_audio_buffer_t{
            .data32 = nullptr,
            .data64 = output_double_channels.data(),
            .channel_count = 1,
            .latency = 22,
            .constant_mask = 0,
        },
    };

    clap_event_transport_t transport{};
    clap_input_events_t input_events{};
    clap_output_events_t output_events{};
    clap_process_t process{
        .steady_time = -1,
        .frames_count = static_cast<uint32_t>(kFrames),
        .transport = &transport,
        .audio_inputs = inputs.data(),
        .audio_outputs = outputs.data(),
        .audio_inputs_count = static_cast<uint32_t>(inputs.size()),
        .audio_outputs_count = static_cast<uint32_t>(outputs.size()),
        .in_events = &input_events,
        .out_events = &output_events,
    };
};

template <typename Buffer>
concept WritableSamples = requires(Buffer& buffer) {
    *buffer.channelSamples(0) = 1.0f;
};

using InputView = decltype(std::declval<const ProcessContext&>().input<float>());
using OutputView = decltype(std::declval<ProcessContext&>().output<float>());

static_assert(std::same_as<InputView, BufferView<const float>>);
static_assert(std::same_as<OutputView, BufferView<float>>);
static_assert(!WritableSamples<InputView>);
static_assert(WritableSamples<OutputView>);

constexpr clap_plugin_descriptor_t kPluginDescriptor{};

struct StatusPlugin final : PluginBase {
    explicit StatusPlugin(ProcessStatus initial_status)
        : PluginBase(&kPluginDescriptor, nullptr), status{initial_status} {}

    ProcessStatus process(ProcessContext& context) noexcept override {
        observed_process = &context.native();
        return status;
    }

    ProcessStatus status;
    const clap_process_t* observed_process = nullptr;
};

struct MissingProcessPlugin final : PluginBase {
    MissingProcessPlugin() : PluginBase(&kPluginDescriptor, nullptr) {}
};

}  // namespace

TEST_CASE("ProcessContext exposes the wrapped CLAP process",
          "[core][process-context]") {
    ProcessFixture fixture;
    ProcessContext context{fixture.process};

    REQUIRE(context.numFrames() == kFrames);
    REQUIRE(context.steadyTime() == -1);
    REQUIRE(context.transport() == &fixture.transport);
    REQUIRE(context.inputEvents() == &fixture.input_events);
    REQUIRE(context.outputEvents() == &fixture.output_events);
    REQUIRE(&context.native() == &fixture.process);

    const auto inputs = context.audioInputs();
    REQUIRE(inputs.size() == fixture.inputs.size());
    REQUIRE(inputs.data() == fixture.inputs.data());
    REQUIRE(inputs[0].latency == 11);
    REQUIRE(inputs[0].constant_mask == 0b01);

    auto outputs = context.audioOutputs();
    REQUIRE(outputs.size() == fixture.outputs.size());
    REQUIRE(outputs.data() == fixture.outputs.data());
    outputs[1].constant_mask = 0b1;
    REQUIRE(fixture.outputs[1].constant_mask == 0b1);
}

TEST_CASE("ProcessContext creates typed views for each audio port",
          "[core][process-context]") {
    ProcessFixture fixture;
    ProcessContext context{fixture.process};

    SECTION("the default port selects float input and output buffers") {
        auto input = context.input<float>();
        auto output = context.output<float>();

        REQUIRE(input.numChannels() == 2);
        REQUIRE(input.numFrames() == kFrames);
        REQUIRE(input.channelSamples(0) == fixture.input_left.data());
        REQUIRE(input.channelSamples(1) == fixture.input_right.data());
        REQUIRE(input.load(1, 2) == 7.0f);

        REQUIRE(output.numChannels() == 2);
        REQUIRE(output.numFrames() == kFrames);
        REQUIRE(output.channelSamples(0) == fixture.output_left.data());
        REQUIRE(output.channelSamples(1) == fixture.output_right.data());
        output.store(1, 3, 42.0f);
        REQUIRE(fixture.output_right[3] == 42.0f);
    }

    SECTION("an explicit port selects double input and output buffers") {
        auto input = context.input<double>(1);
        auto output = context.output<double>(1);

        REQUIRE(input.numChannels() == 1);
        REQUIRE(input.numFrames() == kFrames);
        REQUIRE(input.channelSamples(0) == fixture.input_double.data());
        REQUIRE(input.load(0, 1) == 10.0);

        REQUIRE(output.numChannels() == 1);
        REQUIRE(output.numFrames() == kFrames);
        REQUIRE(output.channelSamples(0) == fixture.output_double.data());
        output.store(0, 2, 84.0);
        REQUIRE(fixture.output_double[2] == 84.0);
    }

    SECTION("the runtime channel count is not capped by the view type") {
        constexpr std::size_t channel_count = 9;
        std::array<std::array<float, kFrames>, channel_count> input_samples{};
        std::array<std::array<float, kFrames>, channel_count> output_samples{};
        std::array<float*, channel_count> input_channels{};
        std::array<float*, channel_count> output_channels{};

        for (std::size_t channel = 0; channel < channel_count; ++channel) {
            input_channels[channel] = input_samples[channel].data();
            output_channels[channel] = output_samples[channel].data();
        }

        fixture.inputs[0].data32 = input_channels.data();
        fixture.inputs[0].channel_count =
            static_cast<uint32_t>(channel_count);
        fixture.outputs[0].data32 = output_channels.data();
        fixture.outputs[0].channel_count =
            static_cast<uint32_t>(channel_count);

        const auto input = context.input<float>();
        auto output = context.output<float>();

        REQUIRE(input.numChannels() == channel_count);
        REQUIRE(input.channelSamples(channel_count - 1) ==
                input_samples.back().data());
        REQUIRE(output.numChannels() == channel_count);
        REQUIRE(output.channelSamples(channel_count - 1) ==
                output_samples.back().data());
    }
}

TEST_CASE("ProcessContext returns empty views for unavailable audio",
          "[core][process-context]") {
    ProcessFixture fixture;
    ProcessContext context{fixture.process};

    SECTION("missing ports") {
        const auto input = context.input<float>(fixture.inputs.size());
        const auto output = context.output<float>(fixture.outputs.size());
        REQUIRE(input.numChannels() == 0);
        REQUIRE(input.numFrames() == 0);
        REQUIRE(output.numChannels() == 0);
        REQUIRE(output.numFrames() == 0);
    }

    SECTION("the requested precision is unavailable") {
        const auto input = context.input<double>();
        const auto output = context.output<double>();
        REQUIRE(input.numChannels() == 0);
        REQUIRE(input.numFrames() == 0);
        REQUIRE(output.numChannels() == 0);
        REQUIRE(output.numFrames() == 0);
    }

    SECTION("the sample buffer array is null") {
        fixture.inputs[0].data32 = nullptr;
        fixture.outputs[0].data32 = nullptr;

        const auto input = context.input<float>();
        const auto output = context.output<float>();
        REQUIRE(input.numChannels() == 0);
        REQUIRE(input.numFrames() == 0);
        REQUIRE(output.numChannels() == 0);
        REQUIRE(output.numFrames() == 0);
    }

    SECTION("an individual channel buffer is null") {
        fixture.input_float_channels[1] = nullptr;
        fixture.output_float_channels[1] = nullptr;

        const auto input = context.input<float>();
        const auto output = context.output<float>();
        REQUIRE(input.numChannels() == 0);
        REQUIRE(input.numFrames() == 0);
        REQUIRE(output.numChannels() == 0);
        REQUIRE(output.numFrames() == 0);
    }
}

TEST_CASE("ProcessContext handles null CLAP audio arrays",
          "[core][process-context]") {
    ProcessFixture fixture;
    fixture.process.audio_inputs = nullptr;
    fixture.process.audio_outputs = nullptr;
    ProcessContext context{fixture.process};

    REQUIRE(context.audioInputs().empty());
    REQUIRE(context.audioOutputs().empty());
    REQUIRE(context.input<float>().numChannels() == 0);
    REQUIRE(context.output<float>().numChannels() == 0);
}

TEST_CASE("PluginBase dispatches ProcessStatus values to CLAP",
          "[core][process-context][plugin]") {
    constexpr std::array mappings{
        std::pair{ProcessStatus::Error, CLAP_PROCESS_ERROR},
        std::pair{ProcessStatus::Continue, CLAP_PROCESS_CONTINUE},
        std::pair{ProcessStatus::ContinueIfNotQuiet,
                  CLAP_PROCESS_CONTINUE_IF_NOT_QUIET},
        std::pair{ProcessStatus::Tail, CLAP_PROCESS_TAIL},
        std::pair{ProcessStatus::Sleep, CLAP_PROCESS_SLEEP},
    };

    clap_process_t process{};
    StatusPlugin plugin{ProcessStatus::Error};

    for (const auto& [status, clap_status] : mappings) {
        plugin.status = status;
        plugin.observed_process = nullptr;

        REQUIRE(plugin.clapPlugin()->process(plugin.clapPlugin(), &process) ==
                clap_status);
        REQUIRE(plugin.observed_process == &process);
    }
}

TEST_CASE("PluginBase rejects a null CLAP process pointer",
          "[core][process-context][plugin]") {
    StatusPlugin plugin{ProcessStatus::Continue};

    REQUIRE(plugin.clapPlugin()->process(plugin.clapPlugin(), nullptr) ==
            CLAP_PROCESS_ERROR);
    REQUIRE(plugin.observed_process == nullptr);
}

TEST_CASE("PluginBase reports an error when process is not overridden",
          "[core][process-context][plugin]") {
    clap_process_t process{};
    MissingProcessPlugin plugin;

    REQUIRE(plugin.clapPlugin()->process(plugin.clapPlugin(), &process) ==
            CLAP_PROCESS_ERROR);
}
