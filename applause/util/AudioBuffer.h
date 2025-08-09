#pragma once

#include <applause/util/DebugHelpers.h>
#include <cstddef>
#include <span>
#include <xsimd/xsimd.hpp>

namespace applause
{
    /**
     * Non-owning audio buffer that provides typed access to pre-allocated memory.
     *
     * For scalar types: Each index represents a single sample
     * For SIMD types: Each index represents SimdWidth parallel samples
     *
     * Example usage:
     * @code
     * // Scalar buffer for mono processing
     * AudioBuffer<float> monoBuffer(memory_ptr, 1024);
     * monoBuffer[0] = 0.5f;
     *
     * // SIMD buffer for 4 parallel voices
     * AudioBuffer<xsimd::batch<float, 4>> voiceBuffer(memory_ptr, 256);
     * voiceBuffer[0] = {0.1f, 0.2f, 0.3f, 0.4f};
     * @endcode
     *
     */
    template <typename SampleType>
    class AudioBuffer
    {
    public:
        using value_type = SampleType;  // Keep this one for generic code

        static constexpr bool is_simd = xsimd::is_batch<SampleType>::value;
        static constexpr size_t simd_width = is_simd ? SampleType::size : 1;

        /**
         * Constructs a buffer view over pre-allocated memory.
         *
         * @param data Pointer to aligned memory (must be 64-byte aligned for SIMD)
         * @param frame_count Number of logical samples (for SIMD, number of batches)
         */
        explicit AudioBuffer(float* data, size_t frame_count) noexcept
            : data_(reinterpret_cast<SampleType*>(data))
              , frame_count_(frame_count)
        {
            ASSERT(data != nullptr, "AudioBuffer: data pointer is null");
            ASSERT(frame_count > 0, "AudioBuffer: frame count must be greater than zero");
            // Verify alignment for SIMD types
            if constexpr (is_simd)
            {
                ASSERT(reinterpret_cast<uintptr_t>(data) % 64 == 0, "AudioBuffer: data pointer is not 64-byte aligned");
            }
        }

        /**
         * Constructs a buffer from a span of floats.
         * For SIMD types, the span size must be divisible by simd_width.
         */
        explicit AudioBuffer(std::span<float> memory) noexcept
            : AudioBuffer(memory.data(), memory.size() / simd_width)
        {
            if constexpr (is_simd)
            {
                ASSERT(memory.size() % simd_width == 0, "AudioBuffer: span size is not divisible by simd width");
            }
        }

        SampleType& operator[](size_t idx) noexcept
        {
            ASSERT(idx < frame_count_, "AudioBuffer: index out of bounds");
            return data_[idx];
        }

        const SampleType& operator[](size_t idx) const noexcept
        {
            ASSERT(idx < frame_count_, "AudioBuffer: index out of bounds");
            return data_[idx];
        }

        [[nodiscard]] size_t size() const noexcept { return frame_count_; }

        /**
         * Returns total number of float values in the buffer.
         * For scalar: same as size()
         * For SIMD: size() * simd_width
         */
        [[nodiscard]] size_t floatCount() const noexcept
        {
            return frame_count_ * simd_width;
        }

        SampleType* data() noexcept { return data_; }
        const SampleType* data() const noexcept { return data_; }

        /**
         * Returns raw float pointer to underlying memory.
         * Useful for passing to C APIs or memory operations.
         */
        float* rawData() noexcept
        {
            return reinterpret_cast<float*>(data_);
        }

        const float* rawData() const noexcept
        {
            return reinterpret_cast<const float*>(data_);
        }

        SampleType* begin() noexcept { return data_; }
        SampleType* end() noexcept { return data_ + frame_count_; }
        const SampleType* begin() const noexcept { return data_; }
        const SampleType* end() const noexcept { return data_ + frame_count_; }
        /**
         * Clears the buffer (sets all values to zero).
         */
        void clear() noexcept
        {
            memset(data_, 0, sizeof(SampleType) * frame_count_);
        }


    private:
        SampleType* data_;
        size_t frame_count_;
    };

    // Type aliases for common use cases
    using ScalarAudioBuffer = AudioBuffer<float>;
    using SimdAudioBuffer = AudioBuffer<xsimd::batch<float, xsimd::default_arch>>;
} // namespace applause
