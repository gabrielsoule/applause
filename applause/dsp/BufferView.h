#pragma once

#include <applause/util/DebugHelpers.h>
#include <applause/util/SampleType.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>

namespace applause
{
    /**
     * Non-owning view over an interleaved block of channel planes. For example, a stereo buffer would be divided into
     * two sections of memory, one for each channel. In each section, samples are stored in time order. Samples can be either
     * raw floats/doubles or SIMD batches.
     *
     * It is recommended that a MemoryArena be used to allocate memory for this class. It is the responsibility
     * of the developer to ensure that underlying memory is allocated and freed in a way that is compatible with the
     * lifetime of the BufferView.
     */
    template <SampleConcept S, std::size_t Channels = 2>
    class BufferView
    {
    public:
        using Sample = S;
        using Scalar = scalar_t<S>;

        static constexpr std::size_t channel_count = Channels;
        static constexpr std::size_t sample_width = sampleWidth<Sample>();
        static constexpr bool is_simd = SimdBatch<Sample>;

        /**
         * A lightweight view over a single channel. Grabbing one of these, and then iterating through all the samples
         * in a channel, is marginally more efficient using the buffer's load/store function, since you only need
         * to compute the channel offset once.
         */
        class ChannelView
        {
        public:
            constexpr ChannelView(Scalar* base, std::size_t frames) noexcept
                : base_{base}, frame_count_{frames}
            {
            }

            [[nodiscard]] Sample load(std::size_t frame) const noexcept
            {
                ASSERT(frame < frame_count_, "ChannelAccessor: frame out of range");
                return applause::load_unaligned<Sample>(base_ + frame * sample_width);
            }

            void store(std::size_t frame, const Sample& value) noexcept
            {
                ASSERT(frame < frame_count_, "ChannelAccessor: frame out of range");
                applause::store_unaligned(value, base_ + frame * sample_width);
            }

            [[nodiscard]] Scalar* framePtr(std::size_t frame) noexcept
            {
                ASSERT(frame < frame_count_, "ChannelAccessor: frame out of range");
                return base_ + frame * sample_width;
            }

            [[nodiscard]] const Scalar* framePtr(std::size_t frame) const noexcept
            {
                ASSERT(frame < frame_count_, "ChannelAccessor: frame out of range");
                return base_ + frame * sample_width;
            }

            [[nodiscard]] Scalar* data() noexcept { return base_; }
            [[nodiscard]] const Scalar* data() const noexcept { return base_; }
            [[nodiscard]] std::size_t frames() const noexcept { return frame_count_; }

        private:
            Scalar* base_;
            std::size_t frame_count_;
        };

        constexpr BufferView() noexcept = default;

        /**
         * Constructor for contiguous memory layout (interleaved channel planes).
         * Channels are stored sequentially: all of channel 0, then all of channel 1, etc.
         */
        constexpr BufferView(Scalar* base_ptr,
                             std::size_t frame_count) noexcept
            : base_ptr_{base_ptr}
              , frame_count_{frame_count}
              , is_contiguous_{true}
        {
            ASSERT(base_ptr_ != nullptr || frame_count_ == 0,
                   "BufferView: null base pointer with nonzero frame count");
            if (base_ptr_ != nullptr)
            {
                ASSERT(reinterpret_cast<std::uintptr_t>(base_ptr_) % alignof(Sample) == 0,
                       "BufferView: base pointer not aligned for sample type");

                // Pre-calculate channel pointers for contiguous layout
                for (std::size_t ch = 0; ch < channel_count; ++ch)
                {
                    channel_ptrs_[ch] = base_ptr_ + ch * frame_count_ * sample_width;
                }
            }
        }

        /**
         * Convenience constructor for hosts that expose CLAP-style `float**` buffers.
         * Each pointer represents one channel plane that may live at an arbitrary location.
         * If the planes are actually packed back-to-back, the view will automatically upgrade
         * to the contiguous fast path.
         *
         * @param channels_ptr Pointer to the array of per-channel buffers supplied by the host
         * @param frame_count  Number of frames available in each channel
         */
        template <typename T = Scalar, std::enable_if_t<std::is_same_v<T, float>, int> = 0>
        constexpr BufferView(Scalar** channels_ptr,
                             std::size_t frame_count) noexcept
            : base_ptr_{nullptr}
              , frame_count_{frame_count}
              , is_contiguous_{false}
        {
            ASSERT(frame_count > 0, "frame count must be greater than zero");
            ASSERT(channels_ptr != nullptr, "null channel pointer array");
            for (std::size_t c = 0; c < channel_count; ++c)
            {
                ASSERT(channels_ptr[c] != nullptr, "null channel pointer");
            }

            // Copy channel pointers supplied by the host
            for (std::size_t ch = 0; ch < channel_count; ++ch)
            {
                channel_ptrs_[ch] = channels_ptr[ch];
            }

            verifyContiguity();
        }

        [[nodiscard]] constexpr std::size_t numFrames() const noexcept { return frame_count_; }
        [[nodiscard]] std::size_t scalarsPerChannel() const noexcept { return frame_count_ * sample_width; }
        [[nodiscard]] constexpr std::size_t samplesPerChannel() const noexcept { return frame_count_; }
        [[nodiscard]] constexpr std::size_t numChannels() const noexcept { return channel_count; }

        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return is_contiguous_ ? (base_ptr_ != nullptr) : (channel_ptrs_[0] != nullptr);
        }

        [[nodiscard]] constexpr bool isContiguous() const noexcept { return is_contiguous_; }

        [[nodiscard]] Scalar* rawData() noexcept { return base_ptr_; }
        [[nodiscard]] const Scalar* rawData() const noexcept { return base_ptr_; }

        [[nodiscard]] Scalar* channelScalars(std::size_t channel) noexcept
        {
            ASSERT(channel < channel_count, "BufferView: channel index out of range");
            return channel_ptrs_[channel];
        }

        [[nodiscard]] const Scalar* channelScalars(std::size_t channel) const noexcept
        {
            ASSERT(channel < channel_count, "BufferView: channel index out of range");
            return channel_ptrs_[channel];
        }

        [[nodiscard]] std::span<Scalar> channelScalarSpan(std::size_t channel) noexcept
        {
            Scalar* ptr = channelScalars(channel);
            if (frame_count_ == 0 || ptr == nullptr)
            {
                return {};
            }
            return {ptr, scalarsPerChannel()};
        }

        [[nodiscard]] std::span<const Scalar> channelScalarSpan(std::size_t channel) const noexcept
        {
            const Scalar* ptr = channelScalars(channel);
            if (frame_count_ == 0 || ptr == nullptr)
            {
                return {};
            }
            return {ptr, scalarsPerChannel()};
        }

        /**
         * Extracts a subview from the current buffer view, defined by a specified range of frames.
         * This method returns a new BufferView object that represents a subset of the frames in the
         * original buffer view, starting at `start_frame` and ending at `end_frame` (exclusive).
         *
         * The subview will still have the same sample type and channel count as its parent; this function only
         * slices across the frame (time) axis. The subview will be contiguous in memory if the parent is contiguous.
         *
         * @param start_frame The index of the first frame in the subview. Must be less than or equal to `end_frame`.
         * @param end_frame The index that is one past the last frame in the subview. Must be less than or equal to `frame_count_`.
         * @return A new BufferView object representing the specified frame range within the current buffer view.
         */
        [[nodiscard]] constexpr BufferView getSubView(std::size_t start_frame,
                                                      std::size_t end_frame) const noexcept
        {
            ASSERT(start_frame <= end_frame, "BufferView::getSubView: invalid frame range");
            ASSERT(end_frame <= frame_count_, "BufferView::getSubView: end frame out of range");

            BufferView sub{};
            const std::size_t sub_frames = end_frame - start_frame;
            sub.frame_count_ = sub_frames;

            // Fast return: full-range slice => identical view.
            if (start_frame == 0 && sub_frames == frame_count_)
                return *this;

            ASSERT(start_frame <= std::numeric_limits<std::size_t>::max() / sample_width,
                   "frame offset overflow");

            const std::size_t frame_scalar_offset = start_frame * sample_width;

            // Always compute per-channel pointers (works for both layouts).
            if (frame_scalar_offset == 0)
            {
                // Cheap copy when we're slicing only at the tail (start==0).
                sub.channel_ptrs_ = channel_ptrs_;
            }
            else
            {
                for (std::size_t ch = 0; ch < channel_count; ++ch)
                {
                    Scalar* src = channel_ptrs_[ch];
                    sub.channel_ptrs_[ch] = src ? src + frame_scalar_offset : nullptr;
                }
            }

            // Decide contiguity & base pointer.
            if constexpr (channel_count == 1)
            {
                // One plane: any slice is contiguous.
                sub.is_contiguous_ = (sub.channel_ptrs_[0] != nullptr);
                sub.base_ptr_ = sub.channel_ptrs_[0];
            }
            else
            {
                // Multi-channel: only a full-range slice preserves global contiguity.
                const bool full_range = (sub_frames == frame_count_);
                const bool parent_contiguous = is_contiguous_ && (base_ptr_ != nullptr);

                sub.is_contiguous_ = full_range && parent_contiguous;
                sub.base_ptr_ = sub.is_contiguous_ ? base_ptr_ /* + 0 */ : nullptr;
            }

            return sub;
        }

        [[nodiscard]] Scalar* frameScalars(std::size_t channel, std::size_t frame) noexcept
        {
            Scalar* channel_ptr = channelScalars(channel);
            return channel_ptr ? channel_ptr + frame * sample_width : nullptr;
        }

        [[nodiscard]] const Scalar* frameScalars(std::size_t channel, std::size_t frame) const noexcept
        {
            const Scalar* channel_ptr = channelScalars(channel);
            return channel_ptr ? channel_ptr + frame * sample_width : nullptr;
        }

        [[nodiscard]] Sample load(std::size_t channel, std::size_t frame) const noexcept
        {
            const Scalar* ptr = frameScalars(channel, frame);
            return applause::load_unaligned<Sample>(ptr);
        }

        void store(std::size_t channel, std::size_t frame, const Sample& value) noexcept
        {
            Scalar* ptr = frameScalars(channel, frame);
            applause::store_unaligned<Sample>(value, ptr);
        }

        /**
         * Clears (zeros) all samples in the buffer.
         */
        void clear() noexcept
        {
            if (frame_count_ == 0)
                return;

            if (is_contiguous_ && base_ptr_ != nullptr)
            {
                // Fast path: single memset for contiguous memory
                const std::size_t total_scalars = channel_count * scalarsPerChannel();
                std::memset(base_ptr_, 0, total_scalars * sizeof(Scalar));
            }
            else
            {
                // Non-contiguous: clear each channel separately
                for (std::size_t ch = 0; ch < channel_count; ++ch)
                {
                    if (channel_ptrs_[ch] != nullptr)
                    {
                        std::memset(channel_ptrs_[ch], 0, scalarsPerChannel() * sizeof(Scalar));
                    }
                }
            }
        }

        /**
         * Clears (zeros) a single channel.
         */
        void clearChannel(std::size_t channel) noexcept
        {
            ASSERT(channel < channel_count, "BufferView: channel index out of range");
            Scalar* channel_ptr = channelScalars(channel);
            if (channel_ptr == nullptr)
                return;

            std::memset(channel_ptr, 0, scalarsPerChannel() * sizeof(Scalar));
        }

        [[nodiscard]] ChannelView channel(std::size_t ch) noexcept
        {
            return ChannelView(channelScalars(ch), frame_count_);
        }

        [[nodiscard]] const ChannelView channel(std::size_t ch) const noexcept
        {
            return ChannelView(const_cast<Scalar*>(channelScalars(ch)), frame_count_);
        }

    private:
        Scalar* base_ptr_ = nullptr;
        std::size_t frame_count_ = 0;
        std::array<Scalar*, Channels> channel_ptrs_{};
        bool is_contiguous_ = true;

        bool verifyContiguity() noexcept
        {
            if (is_contiguous_) return true;
            const std::uintptr_t stride_bytes = static_cast<std::uintptr_t>(scalarsPerChannel()) * sizeof(Scalar);

            const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(channel_ptrs_[0]);

            // Enforce the same alignment guarantee as the contiguous constructor.
            if (base % alignof(Sample) != 0)
                return false;

            // Expected address of the next channel plane if memory is truly contiguous.
            std::uintptr_t expected = base + stride_bytes;

            // Verify every subsequent channel starts exactly at the expected address.
            for (std::size_t ch = 1; ch < channel_count; ++ch)
            {
                const std::uintptr_t p = reinterpret_cast<std::uintptr_t>(channel_ptrs_[ch]);
                if (p != expected)
                    return false;

                expected += stride_bytes;
            }

            // Success!! :) Promote to fast-path contiguous layout.
            base_ptr_ = channel_ptrs_[0];
            is_contiguous_ = true;
            return true;
        }
    };
}
