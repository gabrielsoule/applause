#pragma once

#include <applause/util/DebugHelpers.h>
#include <applause/util/SampleType.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

namespace applause {
/**
 * Non-owning view over a planar audio buffer with capacity for MaxChannels.
 * The active channel count is tracked at runtime and may be smaller than that
 * capacity. Each channel contains samples in time order and may live at an
 * arbitrary memory location.
 *
 * Samples can be either raw floats/doubles or SIMD batches. A SIMD batch is
 * one time-domain frame whose lanes represent parallel streams such as voices.
 *
 * It is recommended that a MemoryArena be used to allocate memory for this
 * class. It is the responsibility of the developer to ensure that underlying
 * memory is allocated and freed in a way that is compatible with the lifetime
 * of the BufferView.
 */
template <typename S, std::size_t MaxChannels = 8>
    requires Sample<std::remove_const_t<S>>
class BufferView {
public:
    using Sample = S;
    using Value = std::remove_const_t<Sample>;
    using Scalar = scalar_t<Value>;
    using ScalarElement =
        std::conditional_t<std::is_const_v<Sample>, const Scalar, Scalar>;

    static constexpr std::size_t max_channel_count = MaxChannels;
    static constexpr std::size_t sample_width = sampleWidth<Value>();
    static constexpr bool is_simd = SimdBatch<Value>;
    static_assert(sample_width * sizeof(Scalar) == sizeof(Value),
                  "BufferView requires Sample to be tightly packed scalars");

    /**
     * A lightweight view over a single channel. Grabbing one of these, and then
     * iterating through all the samples in a channel, is marginally more
     * efficient using the buffer's load/store function, since you only need to
     * compute the channel offset once.
     */
    class ChannelView {
    public:
        constexpr ChannelView(Sample* base, std::size_t frames) noexcept
            : base_{base}, frame_count_{frames} {}

        [[nodiscard]] Value load(std::size_t frame) const noexcept {
            ASSERT(frame < frame_count_, "ChannelView: frame out of range");
            return base_[frame];
        }

        void store(std::size_t frame, const Value& value) const noexcept
            requires(!std::is_const_v<Sample>) {
            ASSERT(frame < frame_count_, "ChannelView: frame out of range");
            base_[frame] = value;
        }

        /**
         * Adds a value to an existing sample in the channel.
         * Equivalent to: sample = load(frame) + value, but more efficient.
         */
        void add(std::size_t frame, const Value& value) const noexcept
            requires(!std::is_const_v<Sample>) {
            ASSERT(frame < frame_count_, "ChannelView: frame out of range");
            base_[frame] += value;
        }

        [[nodiscard]] Sample* samplePtr(std::size_t frame) const noexcept {
            ASSERT(frame < frame_count_, "ChannelView: frame out of range");
            return base_ + frame;
        }

        [[nodiscard]] ScalarElement* framePtr(
            std::size_t frame) const noexcept {
            ASSERT(frame < frame_count_, "ChannelView: frame out of range");
            return reinterpret_cast<ScalarElement*>(base_ + frame);
        }

        [[nodiscard]] Sample* data() const noexcept { return base_; }
        [[nodiscard]] std::size_t frames() const noexcept {
            return frame_count_;
        }

    private:
        Sample* base_ = nullptr;
        std::size_t frame_count_ = 0;
    };

    constexpr BufferView() noexcept = default;

    /**
     * Constructor for contiguous memory layout (sequential channel planes)
     * Channels are stored sequentially: all of channel 0, then all of channel
     * 1, etc.
     */
    constexpr BufferView(ScalarElement* base_ptr, std::size_t channel_count,
                         std::size_t frame_count) noexcept {
        if (channel_count > MaxChannels) {
            LOG_ERR("BufferView: channel count {} exceeds capacity {}",
                    channel_count, MaxChannels);
            return;
        }
        if (base_ptr == nullptr && channel_count != 0 && frame_count != 0) {
            LOG_ERR(
                "BufferView: null base pointer with nonzero channel and frame counts");
            return;
        }
        if (base_ptr != nullptr &&
            reinterpret_cast<std::uintptr_t>(base_ptr) % alignof(Value) != 0) {
            LOG_ERR("BufferView: base pointer not aligned for sample type");
            return;
        }

        frame_count_ = frame_count;
        active_channels_ = channel_count;
        Sample* base_sample = reinterpret_cast<Sample*>(base_ptr);
        for (std::size_t ch = 0; ch < active_channels_; ++ch) {
            channel_ptrs_[ch] =
                base_sample ? base_sample + ch * frame_count_ : nullptr;
        }
        for (std::size_t ch = active_channels_; ch < MaxChannels; ++ch) {
            channel_ptrs_[ch] = nullptr;
        }
    }

    /**
     * Compatibility overload that assumes all template channels are active.
     */
    constexpr BufferView(ScalarElement* base_ptr,
                         std::size_t frame_count) noexcept
        : BufferView(base_ptr, MaxChannels, frame_count) {}

    /**
     * Convenience constructor for hosts that expose raw `float**`
     * buffers. Each pointer represents one channel plane that may live at an
     * arbitrary location. Contiguity can be queried with isContiguous().
     *
     * @param channels_ptr Pointer to the array of per-channel buffers supplied
     * by the host
     * @param frame_count  Number of frames available in each channel
     */
    template <typename InputScalar>
        requires std::same_as<std::remove_const_t<InputScalar>, Scalar> &&
                 (std::is_const_v<Sample> || !std::is_const_v<InputScalar>)
    constexpr BufferView(InputScalar* const* channels_ptr,
                         std::size_t channel_count,
                         std::size_t frame_count) noexcept {
        if (channel_count > MaxChannels) {
            LOG_ERR("BufferView: channel count {} exceeds capacity {}",
                    channel_count, MaxChannels);
            return;
        }
        if (channels_ptr == nullptr && channel_count != 0) {
            LOG_ERR("BufferView: null channel pointer array");
            return;
        }
        for (std::size_t c = 0; c < channel_count; ++c) {
            if (channels_ptr[c] == nullptr && frame_count != 0) {
                LOG_ERR("BufferView: null pointer for channel {}", c);
                return;
            }
            if (channels_ptr[c] != nullptr &&
                reinterpret_cast<std::uintptr_t>(channels_ptr[c]) %
                        alignof(Value) !=
                    0) {
                LOG_ERR(
                    "BufferView: channel {} pointer not aligned for sample type",
                    c);
                return;
            }
        }

        frame_count_ = frame_count;
        active_channels_ = channel_count;

        // Copy channel pointers supplied by the host
        using InputSample =
            std::conditional_t<std::is_const_v<InputScalar>, const Value,
                               Value>;
        for (std::size_t ch = 0; ch < active_channels_; ++ch) {
            channel_ptrs_[ch] =
                reinterpret_cast<InputSample*>(channels_ptr[ch]);
        }
        // Zero remaining pointers for safety
        for (std::size_t ch = active_channels_; ch < MaxChannels; ++ch) {
            channel_ptrs_[ch] = nullptr;
        }
    }

    /** Converts a writable view to a read-only view without copying samples. */
    template <typename OtherSample>
        requires std::is_const_v<Sample> &&
                 std::same_as<OtherSample, Value>
    constexpr BufferView(
        const BufferView<OtherSample, MaxChannels>& other) noexcept
        : frame_count_{other.numFrames()},
          active_channels_{other.numChannels()} {
        for (std::size_t ch = 0; ch < active_channels_; ++ch) {
            channel_ptrs_[ch] = other.channelSamples(ch);
        }
    }

    [[nodiscard]] constexpr std::size_t numFrames() const noexcept {
        return frame_count_;
    }
    [[nodiscard]] std::size_t scalarsPerChannel() const noexcept {
        return frame_count_ * sample_width;
    }
    [[nodiscard]] constexpr std::size_t samplesPerChannel() const noexcept {
        return frame_count_;
    }
    [[nodiscard]] constexpr std::size_t numChannels() const noexcept {
        return active_channels_;
    }

    [[nodiscard]] bool isValid() const noexcept {
        if (active_channels_ == 0 || frame_count_ == 0) return true;
        for (std::size_t ch = 0; ch < active_channels_; ++ch) {
            if (channel_ptrs_[ch] == nullptr) return false;
        }
        return true;
    }

    [[nodiscard]] bool isContiguous() const noexcept {
        if (!isValid()) return false;
        if (active_channels_ <= 1 || frame_count_ == 0) return true;
        Sample* base = channel_ptrs_[0];
        for (std::size_t ch = 1; ch < active_channels_; ++ch) {
            if (channel_ptrs_[ch] != base + ch * frame_count_) return false;
        }
        return true;
    }

    [[nodiscard]] Sample* channelSamples(std::size_t channel) noexcept {
        ASSERT(channel < active_channels_,
               "BufferView: channel index out of range");
        return channel_ptrs_[channel];
    }

    [[nodiscard]] Sample* channelSamples(std::size_t channel) const noexcept {
        ASSERT(channel < active_channels_,
               "BufferView: channel index out of range");
        return channel_ptrs_[channel];
    }

    [[nodiscard]] std::span<Sample> channelSampleSpan(
        std::size_t channel) noexcept {
        Sample* ptr = channelSamples(channel);
        if (frame_count_ == 0 || ptr == nullptr) {
            return {};
        }
        return {ptr, frame_count_};
    }

    [[nodiscard]] std::span<Sample> channelSampleSpan(
        std::size_t channel) const noexcept {
        Sample* ptr = channelSamples(channel);
        if (frame_count_ == 0 || ptr == nullptr) {
            return {};
        }
        return {ptr, frame_count_};
    }

    /**
     * Extracts a subview from the current buffer view, defined by a specified
     * range of frames. This method returns a new BufferView object that
     * represents a subset of the frames in the original buffer view, starting
     * at `start_frame` and ending at `end_frame` (exclusive).
     *
     * The subview will still have the same sample type and channel count as its
     * parent; this function only slices across the frame (time) axis. Note that
     * the resulting view is only contiguous when either the original view was
     * contiguous and the slice spans the full frame range, or the view contains
     * a single channel.
     *
     * @param start_frame The index of the first frame in the subview. Must be
     * less than or equal to `end_frame`.
     * @param end_frame The index that is one past the last frame in the
     * subview. Must be less than or equal to `frame_count_`.
     * @return A new BufferView object representing the specified frame range
     * within the current buffer view.
     */
    [[nodiscard]] constexpr BufferView getSubView(
        std::size_t start_frame, std::size_t end_frame) const noexcept {
        ASSERT(start_frame <= end_frame,
               "BufferView::getSubView: invalid frame range");
        ASSERT(end_frame <= frame_count_,
               "BufferView::getSubView: end frame out of range");

        BufferView sub{};
        const std::size_t sub_frames = end_frame - start_frame;
        sub.frame_count_ = sub_frames;
        sub.active_channels_ = active_channels_;

        for (std::size_t ch = 0; ch < active_channels_; ++ch) {
            Sample* src = channel_ptrs_[ch];
            sub.channel_ptrs_[ch] = src ? (src + start_frame) : nullptr;
        }
        for (std::size_t ch = active_channels_; ch < MaxChannels; ++ch) {
            sub.channel_ptrs_[ch] = nullptr;
        }

        return sub;
    }

    [[nodiscard]] Value load(std::size_t channel,
                             std::size_t frame) const noexcept {
        ASSERT(frame < frame_count_, "BufferView::load: frame out of range");
        Sample* ptr = channelSamples(channel);
        ASSERT(ptr != nullptr, "BufferView::load: null channel pointer");
        return ptr[frame];
    }

    void store(std::size_t channel, std::size_t frame,
               const Value& value) const noexcept
        requires(!std::is_const_v<Sample>) {
        ASSERT(frame < frame_count_, "BufferView::store: frame out of range");
        Sample* ptr = channelSamples(channel);
        ASSERT(ptr != nullptr, "BufferView::store: null channel pointer");
        ptr[frame] = value;
    }

    /**
     * Adds a value to an existing sample in the buffer.
     * Equivalent to: store(ch, frame, load(ch, frame) + value), but more efficient.
     */
    void add(std::size_t channel, std::size_t frame,
             Scalar value) const noexcept
        requires(!std::is_const_v<Sample>) {
        ASSERT(frame < frame_count_, "BufferView::add: frame out of range");
        Sample* ptr = channelSamples(channel);
        if (!ptr) return;

        if constexpr (is_simd) {
            ptr[frame] += applause::set1<Value>(value);
        } else {
            ptr[frame] += value;
        }
    }

    /**
     * Clears (zeros) all samples in the buffer.
     */
    void clear() const noexcept
        requires(!std::is_const_v<Sample>) {
        if (frame_count_ == 0) return;
        const std::size_t bytes = scalarsPerChannel() * sizeof(Scalar);
        for (std::size_t ch = 0; ch < active_channels_; ++ch) {
            Sample* ptr = channel_ptrs_[ch];
            if (ptr != nullptr) {
                std::memset(reinterpret_cast<Scalar*>(ptr), 0, bytes);
            }
        }
    }

    /**
     * Clears (zeros) a single channel.
     */
    void clearChannel(std::size_t channel) const noexcept
        requires(!std::is_const_v<Sample>) {
        ASSERT(channel < active_channels_,
               "BufferView: channel index out of range");
        Sample* channel_ptr = channelSamples(channel);
        if (channel_ptr == nullptr) return;

        std::memset(reinterpret_cast<Scalar*>(channel_ptr), 0,
                    scalarsPerChannel() * sizeof(Scalar));
    }

    [[nodiscard]] ChannelView channel(std::size_t ch) noexcept {
        return ChannelView(channelSamples(ch), frame_count_);
    }

    [[nodiscard]] ChannelView channel(std::size_t ch) const noexcept {
        return ChannelView(channelSamples(ch), frame_count_);
    }

private:
    std::size_t frame_count_ = 0;
    std::size_t active_channels_ = 0;
    std::array<Sample*, MaxChannels> channel_ptrs_{};
};

// Common buffer type aliases for convenience
using MonoBuffer = BufferView<float, 1>;
using StereoBuffer = BufferView<float, 2>;
using SurroundBuffer = BufferView<float, 8>;
using FlexBuffer = BufferView<float, 8>;  // Flexible up to 7.1 surround

}  // namespace applause
