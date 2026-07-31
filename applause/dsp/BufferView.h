#pragma once

#include <applause/util/DebugHelpers.h>
#include <applause/util/SampleType.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

namespace applause {
/**
 * Non-owning view over a planar audio buffer. Each channel contains samples in
 * time order and may live at an arbitrary memory location.
 *
 * Samples can be either raw floats/doubles or SIMD batches. A SIMD batch is
 * one time-domain frame whose lanes represent parallel streams such as voices.
 * Storage is always addressed as the underlying scalar type; SIMD values are
 * transferred with explicit aligned loads and stores.
 *
 * BufferView borrows both the channel-pointer table and the sample storage. The
 * table, every pointer stored in it, and all referenced samples must remain
 * valid for the lifetime of the view and any subviews created from it. A
 * MemoryArena can be used to give the table and samples a shared lifetime.
 */
template <typename S>
    requires Sample<std::remove_const_t<S>>
class BufferView {
    template <typename OtherSample>
        requires Sample<std::remove_const_t<OtherSample>>
    friend class BufferView;

public:
    using Sample = S;
    using Value = std::remove_const_t<Sample>;
    using Scalar = scalar_t<Value>;
    using ScalarElement =
        std::conditional_t<std::is_const_v<Sample>, const Scalar, Scalar>;

    static constexpr std::size_t sample_width = sampleWidth<Value>();
    static constexpr bool is_simd = SimdBatch<Value>;
    static_assert(
        sample_width * sizeof(Scalar) % sampleAlignment<Value>() == 0,
        "BufferView requires every scalar-backed frame to remain aligned");

    /**
     * A lightweight view over a single channel. Grabbing one of these, and then
     * iterating through all the samples in a channel, is marginally more
     * efficient than using the buffer's load/store functions, since the channel
     * pointer is resolved only once.
     */
    class ChannelView {
    public:
        constexpr ChannelView(ScalarElement* base,
                              std::size_t frames) noexcept
            : base_{base}, frame_count_{frames} {}

        [[nodiscard]] Value load(std::size_t frame) const noexcept {
            ASSERT(frame < frame_count_, "ChannelView: frame out of range");
            return applause::load_aligned<Value>(
                base_ + frame * sample_width);
        }

        void store(std::size_t frame, const Value& value) const noexcept
            requires(!std::is_const_v<Sample>) {
            ASSERT(frame < frame_count_, "ChannelView: frame out of range");
            applause::store_aligned<Value>(
                value, base_ + frame * sample_width);
        }

        /**
         * Adds a value to an existing sample in the channel.
         * Equivalent to: sample = load(frame) + value, but more efficient.
         */
        void add(std::size_t frame, const Value& value) const noexcept
            requires(!std::is_const_v<Sample>) {
            ASSERT(frame < frame_count_, "ChannelView: frame out of range");
            auto* frame_ptr = base_ + frame * sample_width;
            applause::store_aligned<Value>(
                applause::load_aligned<Value>(frame_ptr) + value,
                frame_ptr);
        }

        /** Broadcasts and adds a scalar value to every lane of a SIMD sample. */
        void add(std::size_t frame, Scalar value) const noexcept
            requires(!std::is_const_v<Sample> && is_simd) {
            add(frame, applause::set1<Value>(value));
        }

        [[nodiscard]] Sample* samplePtr(std::size_t frame) const noexcept
            requires(!is_simd) {
            ASSERT(frame < frame_count_, "ChannelView: frame out of range");
            return base_ + frame;
        }

        [[nodiscard]] ScalarElement* framePtr(
            std::size_t frame) const noexcept {
            ASSERT(frame < frame_count_, "ChannelView: frame out of range");
            return base_ + frame * sample_width;
        }

        [[nodiscard]] ScalarElement* scalarData() const noexcept {
            return base_;
        }

        [[nodiscard]] Sample* data() const noexcept
            requires(!is_simd) {
            return base_;
        }

        [[nodiscard]] std::size_t frames() const noexcept {
            return frame_count_;
        }

    private:
        ScalarElement* base_ = nullptr;
        std::size_t frame_count_ = 0;
    };

    constexpr BufferView() noexcept = default;

    /**
     * Constructs a view over a borrowed table of channel pointers. Each pointer
     * represents one channel plane that may live at an arbitrary location.
     * Contiguity can be queried with isContiguous().
     *
     * @param channels_ptr Borrowed table of per-channel buffers
     * @param channel_count Number of pointers in the table
     * @param frame_count Number of frames available in each channel
     */
    template <typename InputScalar>
        requires std::same_as<std::remove_const_t<InputScalar>, Scalar> &&
                 (std::is_const_v<Sample> || !std::is_const_v<InputScalar>)
    constexpr BufferView(InputScalar* const* channels_ptr,
                         std::size_t channel_count,
                         std::size_t frame_count) noexcept {
        if (channels_ptr == nullptr && channel_count != 0) {
            LOG_ERR("BufferView: null channel pointer array");
            return;
        }
        constexpr std::size_t max_size =
            std::numeric_limits<std::size_t>::max();
        if (frame_count > max_size / sample_width ||
            frame_count * sample_width > max_size / sizeof(Scalar)) {
            LOG_ERR("BufferView: frame count exceeds addressable sample storage");
            return;
        }
        for (std::size_t c = 0; c < channel_count; ++c) {
            if (channels_ptr[c] == nullptr && frame_count != 0) {
                LOG_ERR("BufferView: null pointer for channel {}", c);
                return;
            }
            if (channels_ptr[c] != nullptr &&
                reinterpret_cast<std::uintptr_t>(channels_ptr[c]) %
                        sampleAlignment<Value>() !=
                    0) {
                LOG_ERR(
                    "BufferView: channel {} pointer not aligned for sample type",
                    c);
                return;
            }
        }

        channel_ptrs_ = channels_ptr;
        channel_count_ = channel_count;
        frame_count_ = frame_count;
    }

    /** Converts a writable view to a read-only view without copying samples. */
    template <typename OtherSample>
        requires std::is_const_v<Sample> &&
                 std::same_as<OtherSample, Value>
    constexpr BufferView(
        const BufferView<OtherSample>& other) noexcept
        : channel_ptrs_{other.channel_ptrs_},
          channel_count_{other.channel_count_},
          frame_count_{other.frame_count_},
          frame_offset_{other.frame_offset_} {}

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
        return channel_count_;
    }

    [[nodiscard]] bool isValid() const noexcept {
        if (channel_count_ == 0 || frame_count_ == 0) return true;
        if (channel_ptrs_ == nullptr) return false;
        for (std::size_t ch = 0; ch < channel_count_; ++ch) {
            const Scalar* ptr = channelScalars(ch);
            if (ptr == nullptr ||
                reinterpret_cast<std::uintptr_t>(ptr) %
                        sampleAlignment<Value>() !=
                    0) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool isContiguous() const noexcept {
        if (!isValid()) return false;
        if (channel_count_ <= 1 || frame_count_ == 0) return true;

        if (frame_count_ >
            std::numeric_limits<std::size_t>::max() / sample_width) {
            return false;
        }
        const std::size_t scalar_count = frame_count_ * sample_width;
        if (scalar_count >
            std::numeric_limits<std::uintptr_t>::max() / sizeof(Scalar)) {
            return false;
        }
        const auto channel_bytes =
            static_cast<std::uintptr_t>(scalar_count * sizeof(Scalar));

        const auto base = reinterpret_cast<std::uintptr_t>(channelScalars(0));
        for (std::size_t ch = 1; ch < channel_count_; ++ch) {
            if (ch >
                (std::numeric_limits<std::uintptr_t>::max() - base) /
                    channel_bytes) {
                return false;
            }
            const auto offset = ch * channel_bytes;
            if (reinterpret_cast<std::uintptr_t>(channelScalars(ch)) !=
                    base + offset) {
                return false;
            }
        }
        return true;
    }

    /**
     * Returns the scalar storage for a channel at the first visible frame.
     * SIMD frames occupy sample_width consecutive scalar elements.
     */
    [[nodiscard]] ScalarElement* channelScalars(
        std::size_t channel) noexcept {
        ASSERT(channel < channel_count_,
               "BufferView: channel index out of range");
        ScalarElement* ptr = channel_ptrs_[channel];
        return ptr ? ptr + frame_offset_ * sample_width : nullptr;
    }

    [[nodiscard]] ScalarElement* channelScalars(
        std::size_t channel) const noexcept {
        ASSERT(channel < channel_count_,
               "BufferView: channel index out of range");
        ScalarElement* ptr = channel_ptrs_[channel];
        return ptr ? ptr + frame_offset_ * sample_width : nullptr;
    }

    [[nodiscard]] std::span<ScalarElement> channelScalarSpan(
        std::size_t channel) noexcept {
        ScalarElement* ptr = channelScalars(channel);
        if (frame_count_ == 0 || ptr == nullptr) {
            return {};
        }
        return {ptr, scalarsPerChannel()};
    }

    [[nodiscard]] std::span<ScalarElement> channelScalarSpan(
        std::size_t channel) const noexcept {
        ScalarElement* ptr = channelScalars(channel);
        if (frame_count_ == 0 || ptr == nullptr) {
            return {};
        }
        return {ptr, scalarsPerChannel()};
    }

    [[nodiscard]] Sample* channelSamples(std::size_t channel) noexcept
        requires(!is_simd) {
        return channelScalars(channel);
    }

    [[nodiscard]] Sample* channelSamples(std::size_t channel) const noexcept
        requires(!is_simd) {
        return channelScalars(channel);
    }

    [[nodiscard]] std::span<Sample> channelSampleSpan(
        std::size_t channel) noexcept
        requires(!is_simd) {
        return channelScalarSpan(channel);
    }

    [[nodiscard]] std::span<Sample> channelSampleSpan(
        std::size_t channel) const noexcept
        requires(!is_simd) {
        return channelScalarSpan(channel);
    }

    /**
     * Extracts a subview from the current buffer view, defined by a specified
     * range of frames. This method returns a new BufferView object that
     * represents a subset of the frames in the original buffer view, starting
     * at `start_frame` and ending at `end_frame` (exclusive).
     *
     * The subview will still have the same sample type and channel count as its
     * parent; this function only slices across the frame (time) axis.
     * isContiguous() evaluates the selected channel ranges, so a partial slice
     * of an ordinarily contiguous multi-channel buffer is not contiguous.
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

        BufferView sub = *this;
        sub.frame_count_ = end_frame - start_frame;
        sub.frame_offset_ += start_frame;

        return sub;
    }

    [[nodiscard]] Value load(std::size_t channel,
                             std::size_t frame) const noexcept {
        ASSERT(frame < frame_count_, "BufferView::load: frame out of range");
        ScalarElement* ptr = channelScalars(channel);
        ASSERT(ptr != nullptr, "BufferView::load: null channel pointer");
        return applause::load_aligned<Value>(
            ptr + frame * sample_width);
    }

    void store(std::size_t channel, std::size_t frame,
               const Value& value) const noexcept
        requires(!std::is_const_v<Sample>) {
        ASSERT(frame < frame_count_, "BufferView::store: frame out of range");
        ScalarElement* ptr = channelScalars(channel);
        ASSERT(ptr != nullptr, "BufferView::store: null channel pointer");
        applause::store_aligned<Value>(
            value, ptr + frame * sample_width);
    }

    /**
     * Adds a value to an existing sample in the buffer.
     * Equivalent to: store(ch, frame, load(ch, frame) + value), but more efficient.
     */
    void add(std::size_t channel, std::size_t frame,
             const Value& value) const noexcept
        requires(!std::is_const_v<Sample>) {
        ASSERT(frame < frame_count_, "BufferView::add: frame out of range");
        ScalarElement* ptr = channelScalars(channel);
        if (!ptr) return;

        auto* frame_ptr = ptr + frame * sample_width;
        applause::store_aligned<Value>(
            applause::load_aligned<Value>(frame_ptr) + value,
            frame_ptr);
    }

    /** Broadcasts and adds a scalar value to every lane of a SIMD sample. */
    void add(std::size_t channel, std::size_t frame,
             Scalar value) const noexcept
        requires(!std::is_const_v<Sample> && is_simd) {
        add(channel, frame, applause::set1<Value>(value));
    }

    /**
     * Clears (zeros) all samples in the buffer.
     */
    void clear() const noexcept
        requires(!std::is_const_v<Sample>) {
        if (frame_count_ == 0) return;
        const std::size_t bytes = scalarsPerChannel() * sizeof(Scalar);
        for (std::size_t ch = 0; ch < channel_count_; ++ch) {
            ScalarElement* ptr = channelScalars(ch);
            if (ptr != nullptr) {
                std::memset(ptr, 0, bytes);
            }
        }
    }

    /**
     * Clears (zeros) a single channel.
     */
    void clearChannel(std::size_t channel) const noexcept
        requires(!std::is_const_v<Sample>) {
        ASSERT(channel < channel_count_,
               "BufferView: channel index out of range");
        ScalarElement* channel_ptr = channelScalars(channel);
        if (channel_ptr == nullptr) return;

        std::memset(channel_ptr, 0, scalarsPerChannel() * sizeof(Scalar));
    }

    [[nodiscard]] ChannelView channel(std::size_t ch) noexcept {
        return ChannelView(channelScalars(ch), frame_count_);
    }

    [[nodiscard]] ChannelView channel(std::size_t ch) const noexcept {
        return ChannelView(channelScalars(ch), frame_count_);
    }

private:
    ScalarElement* const* channel_ptrs_ = nullptr;
    std::size_t channel_count_ = 0;
    std::size_t frame_count_ = 0;
    std::size_t frame_offset_ = 0;
};

}  // namespace applause
