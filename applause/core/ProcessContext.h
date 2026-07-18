#pragma once

#include <applause/dsp/BufferView.h>
#include <applause/util/DebugHelpers.h>

#include <clap/process.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace applause {

enum class ProcessStatus : int32_t {
    /**
     * Processing failed and the host must discard this block's output; use
     * when the plugin cannot produce valid output for the block.
     */
    Error = CLAP_PROCESS_ERROR,

    /**
     * Processing succeeded and should continue;
     * use while the plugin requires uninterrupted processing.
     */
    Continue = CLAP_PROCESS_CONTINUE,

    /**
     * Processing succeeded; continue only while the output remains non-quiet.
     * Use when processing can stop as soon as the plugin becomes quiet.
     */
    ContinueIfNotQuiet = CLAP_PROCESS_CONTINUE_IF_NOT_QUIET,

    /**
     * Processing succeeded; continue according to the tail length reported by
     * clap_plugin_tail. Use for effects whose remaining work is their tail.
     */
    Tail = CLAP_PROCESS_TAIL,

    /**
     * Processing succeeded and may stop until an event arrives or the audio input
     * changes; use when the plugin is idle and has no tail or other ongoing work.
     */
    Sleep = CLAP_PROCESS_SLEEP,
};

/**
 * Non-owning view of one CLAP process callback.
 *
 * The context and everything obtained from it are valid only for the duration
 * of the process callback.
 */
class ProcessContext {
public:
    /** Creates a non-owning view of the supplied CLAP process data. */
    explicit ProcessContext(const clap_process_t& process) noexcept : process_{process} {}

    /** Returns the number of sample frames in this process block. */
    [[nodiscard]] uint32_t numFrames() const noexcept { return process_.frames_count; }

    /**
     * Returns the host's steady sample-time counter, or -1 when unavailable.
     */
    [[nodiscard]] int64_t steadyTime() const noexcept { return process_.steady_time; }

    /**
     * Returns the transport state at the start of the block, or nullptr when
     * processing without a transport timeline.
     */
    [[nodiscard]] const clap_event_transport_t* transport() const noexcept { return process_.transport; }

    /** Returns the sample-ordered input event list supplied by the host. */
    [[nodiscard]] const clap_input_events_t* inputEvents() const noexcept { return process_.in_events; }

    /** Returns the event list into which the plugin may enqueue output events. */
    [[nodiscard]] const clap_output_events_t* outputEvents() const noexcept { return process_.out_events; }

    /**
     * Returns a read-only view of all audio input ports. Returns an empty span
     * when there are no inputs or the host supplied an invalid port array.
     */
    [[nodiscard]] std::span<const clap_audio_buffer_t> audioInputs() const noexcept {
        if (process_.audio_inputs_count == 0) return {};
        if (process_.audio_inputs == nullptr) {
            LOG_ERR("ProcessContext: null audio input array with {} ports", process_.audio_inputs_count);
            return {};
        }
        return {process_.audio_inputs, process_.audio_inputs_count};
    }

    /**
     * Returns a mutable view of all audio output ports. Returns an empty span
     * when there are no outputs or the host supplied an invalid port array.
     */
    [[nodiscard]] std::span<clap_audio_buffer_t> audioOutputs() noexcept {
        if (process_.audio_outputs_count == 0) return {};
        if (process_.audio_outputs == nullptr) {
            LOG_ERR("ProcessContext: null audio output array with {} ports", process_.audio_outputs_count);
            return {};
        }
        return {process_.audio_outputs, process_.audio_outputs_count};
    }

    /**
     * Returns a read-only sample view of an input port for the requested sample
     * type. Returns an empty view if the port, format, channels, or capacity are
     * unavailable.
     *
     * @tparam T float or double sample type.
     * @tparam ChannelCapacity Maximum number of channels the view can hold.
     * @param port Zero-based input port index; defaults to the first port.
     */
    template <typename T, std::size_t ChannelCapacity>
        requires std::same_as<T, float> || std::same_as<T, double>
    [[nodiscard]] BufferView<const T, ChannelCapacity> input(std::size_t port = 0) const noexcept {
        const auto inputs = audioInputs();
        if (port >= inputs.size()) {
            LOG_ERR("ProcessContext: audio input port {} is unavailable", port);
            return {};
        }

        const auto& buffer = inputs[port];
        if (buffer.channel_count > ChannelCapacity) {
            LOG_ERR("ProcessContext: input port {} has {} channels, capacity is {}", port, buffer.channel_count,
                    ChannelCapacity);
            return {};
        }
        if (buffer.channel_count == 0) return {};

        T* const* channels = nullptr;
        if constexpr (std::same_as<T, float>) {
            channels = buffer.data32;
        } else {
            channels = buffer.data64;
        }

        if (!channelsAvailable(channels, buffer.channel_count)) {
            LOG_ERR("ProcessContext: requested input sample format is unavailable on port {}", port);
            return {};
        }

        return {channels, buffer.channel_count, process_.frames_count};
    }

    /**
     * Returns a writable sample view of an output port for the requested sample
     * type. Returns an empty view if the port, format, channels, or capacity are
     * unavailable.
     *
     * @tparam T float or double sample type.
     * @tparam ChannelCapacity Maximum number of channels the view can hold.
     * @param port Zero-based output port index; defaults to the first port.
     */
    template <typename T, std::size_t ChannelCapacity>
        requires std::same_as<T, float> || std::same_as<T, double>
    [[nodiscard]] BufferView<T, ChannelCapacity> output(std::size_t port = 0) noexcept {
        const auto outputs = audioOutputs();
        if (port >= outputs.size()) {
            LOG_ERR("ProcessContext: audio output port {} is unavailable", port);
            return {};
        }

        auto& buffer = outputs[port];
        if (buffer.channel_count > ChannelCapacity) {
            LOG_ERR("ProcessContext: output port {} has {} channels, capacity is {}", port, buffer.channel_count,
                    ChannelCapacity);
            return {};
        }
        if (buffer.channel_count == 0) return {};

        T* const* channels = nullptr;
        if constexpr (std::same_as<T, float>) {
            channels = buffer.data32;
        } else {
            channels = buffer.data64;
        }

        if (!channelsAvailable(channels, buffer.channel_count)) {
            LOG_ERR("ProcessContext: requested output sample format is unavailable on port {}", port);
            return {};
        }

        return {channels, buffer.channel_count, process_.frames_count};
    }

    /** Returns the underlying CLAP process structure. */
    [[nodiscard]] const clap_process_t& native() const noexcept { return process_; }

private:
    /** Returns whether an array contains a non-null pointer for every channel. */
    template <typename T>
    [[nodiscard]] bool channelsAvailable(T* const* channels, uint32_t channel_count) const noexcept {
        if (channel_count == 0) return true;
        if (channels == nullptr) return false;
        for (uint32_t channel = 0; channel < channel_count; ++channel) {
            if (channels[channel] == nullptr) return false;
        }
        return true;
    }

    const clap_process_t& process_;
};

}  // namespace applause
