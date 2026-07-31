#pragma once

#include <applause/dsp/BufferView.h>
#include <applause/util/SampleType.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace applause {
/**
 * Returns the next pointer with a given byte alignment,
 * or the base pointer if it is already aligned.
 */
template <typename Type, typename IntegerType>
static inline Type* snapPointerToAlignment(
    Type* base_pointer, IntegerType alignment_bytes) noexcept {
    return (Type*)((((size_t)base_pointer) + (alignment_bytes - 1)) &
                   ~(alignment_bytes - 1));
}

static constexpr int defaultByteAlignment =
    64;  // Cache line size for optimal SIMD performance

/**
 * A simple memory arena. By default the arena will be
 * backed with a vector of bytes, but the underlying
 * memory resource can be changed via the template argument.
 */
struct MemoryArena {
    MemoryArena() = default;

    /** Constructs the arena with an initial allocated size. */
    explicit MemoryArena(void* data, size_t size_in_bytes) {
        raw_data_ = {
            (std::byte*)data,
            size_in_bytes,
        };
    }

    MemoryArena(const MemoryArena&) = delete;
    MemoryArena& operator=(const MemoryArena&) = delete;

    MemoryArena(MemoryArena&&) noexcept = default;
    MemoryArena& operator=(MemoryArena&&) noexcept = default;

    /**
     * Moves the allocator stack pointer back to zero,
     * effectively "reclaiming" all allocated memory.
     */
    void clear() noexcept {
#ifndef NDEBUG
        std::fill(raw_data_.begin(), raw_data_.begin() + bytes_used_,
                  std::byte{0xDD});
#endif
        bytes_used_ = 0;
    }

    /** Returns the number of bytes currently being used */
    [[nodiscard]] size_t getBytesUsed() const noexcept { return bytes_used_; }

    /**
     * Allocates a given number of bytes.
     * The returned memory will be un-initialized, so be sure to clear it
     * manually if needed.
     */
    void* allocateBytes(size_t num_bytes, size_t alignment = 1) {
        auto* pointer =
            snapPointerToAlignment(raw_data_.data() + bytes_used_, alignment);
        const auto bytes_increment = static_cast<size_t>(
            std::distance(raw_data_.data() + bytes_used_, pointer + num_bytes));

        if (bytes_used_ + bytes_increment > raw_data_.size()) {
            assert(false);
            return nullptr;
        }

        bytes_used_ += bytes_increment;
        return pointer;
    }

    /**
     * Allocates space for some number of objects of type T
     * The returned memory will be un-initialized, so be sure to clear it
     * manually if needed.
     */
    template <typename T, typename IntType>
    T* allocate(IntType num_Ts, size_t alignment = alignof(T)) {
        return static_cast<T*>(
            allocateBytes((size_t)num_Ts * sizeof(T), alignment));
    }

    /**
     * Returns a span of type T, and size count.
     * The returned memory will be un-initialized, so be sure to clear it
     * manually if needed.
     */
    template <typename T, typename IntType>
    auto makeSpan(IntType count, size_t alignment = defaultByteAlignment) {
        return std::span{allocate<T>(count, alignment),
                         static_cast<size_t>(count)};
    }

    /**
     * Allocates a channel-pointer table followed by aligned, contiguous channel
     * planes and returns a view borrowing both. The view and all of its
     * subviews must not outlive this allocation (or the arena frame containing
     * it).
     */
    template <Sample SampleT>
    [[nodiscard]] BufferView<SampleT> allocateAudioBuffer(
        std::size_t channel_count, std::size_t frame_count,
        std::size_t alignment = defaultByteAlignment) {
        if (channel_count == 0 || frame_count == 0) {
            return {};
        }

        if (!std::has_single_bit(alignment)) {
            ASSERT_FALSE("Audio buffer alignment must be a power of two");
            return {};
        }

        using Scalar = scalar_t<SampleT>;
        constexpr std::size_t width = sampleWidth<SampleT>();
        constexpr std::size_t max_size =
            std::numeric_limits<std::size_t>::max();

        if (channel_count > max_size / sizeof(Scalar*)) {
            ASSERT_FALSE("Audio buffer channel-pointer table size overflow");
            return {};
        }
        const std::size_t pointer_table_bytes =
            channel_count * sizeof(Scalar*);

        if (frame_count > max_size / width) {
            ASSERT_FALSE("Audio buffer channel size overflow");
            return {};
        }
        const std::size_t scalars_per_channel = frame_count * width;

        if (channel_count > max_size / scalars_per_channel) {
            ASSERT_FALSE("Audio buffer sample count overflow");
            return {};
        }
        const std::size_t total_scalars =
            channel_count * scalars_per_channel;

        if (total_scalars > max_size / sizeof(Scalar)) {
            ASSERT_FALSE("Audio buffer sample storage size overflow");
            return {};
        }
        const std::size_t sample_bytes = total_scalars * sizeof(Scalar);
        const std::size_t sample_alignment =
            std::max(alignment, sampleAlignment<SampleT>());

        if (bytes_used_ > raw_data_.size()) {
            ASSERT_FALSE("MemoryArena allocation state is invalid");
            return {};
        }

        const std::size_t remaining_bytes = raw_data_.size() - bytes_used_;
        if (remaining_bytes == 0) {
            ASSERT_FALSE("Audio buffer allocation failed: arena exhausted");
            return {};
        }

        auto* current = raw_data_.data() + bytes_used_;
        constexpr std::size_t pointer_alignment = alignof(Scalar*);
        const auto current_address =
            reinterpret_cast<std::uintptr_t>(current);
        const std::size_t allocation_padding =
            (pointer_alignment - current_address % pointer_alignment) %
            pointer_alignment;

        if (allocation_padding > remaining_bytes ||
            pointer_table_bytes > remaining_bytes - allocation_padding) {
            ASSERT_FALSE("Audio buffer allocation failed: arena exhausted");
            return {};
        }

        auto* allocation_start = current + allocation_padding;
        auto* after_pointer_table = allocation_start + pointer_table_bytes;
        const auto samples_address =
            reinterpret_cast<std::uintptr_t>(after_pointer_table);
        const std::size_t sample_padding =
            (sample_alignment - samples_address % sample_alignment) %
            sample_alignment;

        if (pointer_table_bytes > max_size - sample_padding) {
            ASSERT_FALSE("Audio buffer allocation size overflow");
            return {};
        }
        const std::size_t metadata_bytes =
            pointer_table_bytes + sample_padding;

        if (sample_bytes > max_size - metadata_bytes ||
            metadata_bytes + sample_bytes >
                remaining_bytes - allocation_padding) {
            ASSERT_FALSE("Audio buffer allocation failed: arena exhausted");
            return {};
        }
        const std::size_t allocation_bytes = metadata_bytes + sample_bytes;

        auto* allocation = static_cast<std::byte*>(
            allocateBytes(allocation_bytes, pointer_alignment));
        if (allocation == nullptr) {
            return {};
        }

        auto** channels = reinterpret_cast<Scalar**>(allocation);
        auto* samples =
            reinterpret_cast<Scalar*>(allocation + metadata_bytes);
        for (std::size_t channel = 0; channel < channel_count; ++channel) {
            channels[channel] =
                samples + channel * scalars_per_channel;
        }

        return BufferView<SampleT>{channels, channel_count, frame_count};
    }

    /** Returns a pointer to the internal buffer with a given offset in bytes */
    template <typename T, typename IntType>
    T* data(IntType offset_bytes) noexcept {
        return reinterpret_cast<T*>(raw_data_.data() + offset_bytes);
    }

    /**
     * Creates a "frame" for the allocator.
     * Once the frame goes out of scope, the allocator will be reset
     * to whatever it's state was at the beginning of the frame.
     */
    struct Frame {
        Frame() = default;

        explicit Frame(MemoryArena& allocator)
            : alloc_(&allocator), bytes_used_at_start_(alloc_->bytes_used_) {}

        ~Frame() { alloc_->bytes_used_ = bytes_used_at_start_; }

        MemoryArena* alloc_ = nullptr;
        size_t bytes_used_at_start_ = 0;
    };

    /** Creates a frame for this allocator */
    auto createFrame() { return Frame{*this}; }

    void resetToFrame(const Frame& frame) {
        assert(frame.alloc_ == this);
        bytes_used_ = frame.bytes_used_at_start_;
    }

    std::span<std::byte> raw_data_{};
    size_t bytes_used_ = 0;
};
}  // namespace applause
