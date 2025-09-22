#pragma once

#include <applause/dsp/BufferView.h>
#include <applause/util/SampleType.h>

#include <algorithm>
#include <cassert>
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

    /** Allocates contiguous channel planes and returns a BufferView wrapper. */
    template <SampleConcept Sample, std::size_t Channels>
    [[nodiscard]] BufferView<Sample, Channels> allocateAudioBuffer(
        std::size_t frame_count, std::size_t alignment = defaultByteAlignment) {
        ASSERT(Channels > 0, "Channel count must be positive");

        if (frame_count == 0) {
            return BufferView<Sample, Channels>{nullptr, 0};
        }

        using Scalar = scalar_t<Sample>;

        constexpr std::size_t sample_alignment = alignof(Sample);
        const std::size_t effective_alignment =
            std::max(alignment, sample_alignment);

        constexpr std::size_t width = sampleWidth<Sample>();

        ASSERT(frame_count <= std::numeric_limits<std::size_t>::max() / width,
               "Frame count overflow!");
        const std::size_t scalars_per_channel = frame_count * width;
        ASSERT(scalars_per_channel <=
                   std::numeric_limits<std::size_t>::max() / Channels,
               "Sample count overflow!");
        const std::size_t total_scalars = scalars_per_channel * Channels;

        Scalar* storage = allocate<Scalar>(total_scalars, effective_alignment);
        ASSERT(storage != nullptr,
               "Storage allocation failed! This should never happen.");

        return BufferView<Sample, Channels>{storage, frame_count};
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
