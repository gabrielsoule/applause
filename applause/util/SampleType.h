#pragma once
#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <xsimd/xsimd.hpp>

namespace applause {
template <typename T>
concept Scalar = std::same_as<T, float> || std::same_as<T, double>;

template <typename T>
concept SimdBatch =
    xsimd::is_batch<T>::value && Scalar<xsimd::scalar_type_t<T>> &&
    requires {
        requires T::arch_type::supported();
        requires(T::arch_type::alignment() > 0);
        requires(T::size > 0);
    };

/**
 * A concept for a sample type. This can either be a scalar type, e.g. a float
 * or a double, or a SIMD batch type, e.g. a xsimd::batch<float/double>.
 */
template <typename T>
concept Sample = Scalar<T> || SimdBatch<T>;

template <Sample S>
using scalar_t = xsimd::scalar_type_t<S>;

template <Sample S>
using mask_t = xsimd::mask_type_t<S>;

template <Sample S>
constexpr std::size_t sampleWidth() noexcept {
    if constexpr (SimdBatch<S>) {
        return S::size;
    } else {
        return std::size_t{1};
    }
}

template <Sample S>
inline constexpr std::size_t sample_width_v = sampleWidth<S>();

template <Sample S>
constexpr std::size_t sampleAlignment() noexcept {
    if constexpr (SimdBatch<S>) {
        return S::arch_type::alignment();
    } else {
        return alignof(S);
    }
}

template <Sample S>
inline S set1(scalar_t<S> v) noexcept {
    if constexpr (SimdBatch<S>)
        return S(v);
    else
        return v;
}

template <Sample S>
inline S load_unaligned(const scalar_t<S>* p) noexcept {
    if constexpr (SimdBatch<S>)
        return S::load_unaligned(p);
    else
        return *p;
}

template <Sample S>
inline void store_unaligned(const S& v, scalar_t<S>* p) noexcept {
    if constexpr (SimdBatch<S>)
        v.store_unaligned(p);
    else
        *p = v;
}

template <Sample S>
inline S load_aligned(const scalar_t<S>* p) noexcept {
    if constexpr (SimdBatch<S>)
        return S::load_aligned(p);
    else
        return *p;
}

template <Sample S>
inline void store_aligned(const S& v, scalar_t<S>* p) noexcept {
    if constexpr (SimdBatch<S>)
        v.store_aligned(p);
    else
        *p = v;
}

template <Sample S>
inline S fma(const S& a, const S& b, const S& c) noexcept {
    if constexpr (SimdBatch<S>)
        return xsimd::fma(a, b, c);
    else
        return std::fma(a, b, c);
}

template <Sample S>
inline S min(const S& a, const S& b) noexcept {
    if constexpr (SimdBatch<S>)
        return xsimd::min(a, b);
    else
        return std::min(a, b);
}

template <Sample S>
inline S max(const S& a, const S& b) noexcept {
    if constexpr (SimdBatch<S>)
        return xsimd::max(a, b);
    else
        return std::max(a, b);
}

template <Sample S>
inline S abs(const S& a) noexcept {
    if constexpr (SimdBatch<S>)
        return xsimd::abs(a);
    else
        return std::abs(a);
}

template <Sample S>
inline S select(const mask_t<S>& mask, const S& true_val,
                const S& false_val) noexcept {
    if constexpr (SimdBatch<S>)
        return xsimd::select(mask, true_val, false_val);
    else
        return mask ? true_val : false_val;
}
}  // namespace applause
