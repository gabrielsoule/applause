#pragma once
#include <algorithm>
#include <cmath>
#include <concepts>
#include <type_traits>
#include <utility>
#include <xsimd/xsimd.hpp>

namespace applause {
template <typename T>
concept SimdBatch = requires {
    typename T::value_type;
    typename T::arch_type;
    typename T::batch_bool_type;
    { T::size } -> std::convertible_to<std::size_t>;
    T(std::declval<typename T::value_type>());
};

template <typename T>
concept Scalar = (!SimdBatch<T>) && std::floating_point<T>;

/**
 * A concept for a sample type. This can either be a scalar type, e.g. a float
 * or a double, or a SIMD batch type, e.g. a xsimd::batch<float/double>.
 */
template <typename T>
concept Sample = Scalar<T> || SimdBatch<T>;

template <Sample S>
class StereoSample;

template <Sample S>
struct scalar_of {
    using type = S;
};

template <SimdBatch S>
struct scalar_of<S> {
    using type = typename S::value_type;
};

template <Sample S>
struct scalar_of<StereoSample<S>> {
    using type = typename scalar_of<S>::type;
};

template <Sample S>
using scalar_t = typename scalar_of<S>::type;

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

template <typename T>
using mask_t =
    std::conditional_t<SimdBatch<T>, typename T::batch_bool_type, bool>;

template <class T, bool UseSIMD>
using SampleType = std::conditional_t<UseSIMD, xsimd::batch<T>, T>;

template <Sample S>
class StereoSample {
public:
    using sample_type = S;
    using scalar_type = scalar_t<S>;

    static constexpr std::size_t channel_count = 2;

    constexpr StereoSample() = default;
    constexpr StereoSample(const StereoSample&) = default;
    constexpr StereoSample(StereoSample&&) noexcept = default;
    constexpr StereoSample& operator=(const StereoSample&) = default;
    constexpr StereoSample& operator=(StereoSample&&) noexcept = default;

    constexpr StereoSample(S left, S right) noexcept
        : left_{std::move(left)}, right_{std::move(right)} {}

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                    (std::convertible_to<V, S> ||
                     std::convertible_to<V, scalar_type>)
    constexpr explicit StereoSample(V&& value) noexcept
        : left_{toSample(std::forward<V>(value))},
          right_{toSample(std::forward<V>(value))} {}

    constexpr S& left() noexcept { return left_; }
    constexpr const S& left() const noexcept { return left_; }
    constexpr S& right() noexcept { return right_; }
    constexpr const S& right() const noexcept { return right_; }

    constexpr S& operator[](std::size_t channel) noexcept {
        return channel == 0 ? left_ : right_;
    }

    constexpr const S& operator[](std::size_t channel) const noexcept {
        return channel == 0 ? left_ : right_;
    }

    constexpr StereoSample& operator+=(const StereoSample& other) noexcept {
        left_ += other.left_;
        right_ += other.right_;
        return *this;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    constexpr StereoSample& operator+=(V&& value) noexcept {
        const S sample = toSample(std::forward<V>(value));
        left_ += sample;
        right_ += sample;
        return *this;
    }

    constexpr StereoSample& operator-=(const StereoSample& other) noexcept {
        left_ -= other.left_;
        right_ -= other.right_;
        return *this;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    constexpr StereoSample& operator-=(V&& value) noexcept {
        const S sample = toSample(std::forward<V>(value));
        left_ -= sample;
        right_ -= sample;
        return *this;
    }

    constexpr StereoSample& operator*=(const StereoSample& other) noexcept {
        left_ *= other.left_;
        right_ *= other.right_;
        return *this;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    constexpr StereoSample& operator*=(V&& value) noexcept {
        const S sample = toSample(std::forward<V>(value));
        left_ *= sample;
        right_ *= sample;
        return *this;
    }

    constexpr StereoSample& operator/=(const StereoSample& other) noexcept {
        left_ /= other.left_;
        right_ /= other.right_;
        return *this;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    constexpr StereoSample& operator/=(V&& value) noexcept {
        const S sample = toSample(std::forward<V>(value));
        left_ /= sample;
        right_ /= sample;
        return *this;
    }

    friend constexpr StereoSample operator+(StereoSample lhs,
                                            const StereoSample& rhs) noexcept {
        lhs += rhs;
        return lhs;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    friend constexpr StereoSample operator+(StereoSample lhs,
                                            V&& value) noexcept {
        lhs += std::forward<V>(value);
        return lhs;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    friend constexpr StereoSample operator+(V&& value,
                                            StereoSample rhs) noexcept {
        rhs += std::forward<V>(value);
        return rhs;
    }

    friend constexpr StereoSample operator-(StereoSample lhs,
                                            const StereoSample& rhs) noexcept {
        lhs -= rhs;
        return lhs;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    friend constexpr StereoSample operator-(StereoSample lhs,
                                            V&& value) noexcept {
        lhs -= std::forward<V>(value);
        return lhs;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    friend constexpr StereoSample operator-(V&& value,
                                            const StereoSample& rhs) noexcept {
        StereoSample result{std::forward<V>(value)};
        result -= rhs;
        return result;
    }

    friend constexpr StereoSample operator*(StereoSample lhs,
                                            const StereoSample& rhs) noexcept {
        lhs *= rhs;
        return lhs;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    friend constexpr StereoSample operator*(StereoSample lhs,
                                            V&& value) noexcept {
        lhs *= std::forward<V>(value);
        return lhs;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    friend constexpr StereoSample operator*(V&& value,
                                            StereoSample rhs) noexcept {
        rhs *= std::forward<V>(value);
        return rhs;
    }

    friend constexpr StereoSample operator/(StereoSample lhs,
                                            const StereoSample& rhs) noexcept {
        lhs /= rhs;
        return lhs;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    friend constexpr StereoSample operator/(StereoSample lhs,
                                            V&& value) noexcept {
        lhs /= std::forward<V>(value);
        return lhs;
    }

    template <typename V>
        requires(!std::same_as<std::remove_cvref_t<V>, StereoSample>) &&
                (std::convertible_to<V, S> ||
                 std::convertible_to<V, scalar_type>)
    friend constexpr StereoSample operator/(V&& value,
                                            const StereoSample& rhs) noexcept {
        StereoSample result{std::forward<V>(value)};
        result /= rhs;
        return result;
    }

    friend constexpr StereoSample operator-(
        const StereoSample& value) noexcept {
        return StereoSample{-value.left_, -value.right_};
    }

private:
    template <typename V>
    static constexpr S toSample(V&& value) noexcept {
        if constexpr (std::same_as<std::remove_cvref_t<V>, S>) {
            return std::forward<V>(value);
        } else if constexpr (std::convertible_to<V, scalar_type>) {
            if constexpr (SimdBatch<S>) {
                return S{static_cast<scalar_type>(std::forward<V>(value))};
            } else {
                return static_cast<scalar_type>(std::forward<V>(value));
            }
        } else if constexpr (std::convertible_to<V, S>) {
            return static_cast<S>(std::forward<V>(value));
        } else {
            static_assert(
                std::same_as<std::remove_cvref_t<V>, void>,
                "StereoSample cannot convert provided type to sample type");
        }
    }

    S left_{};
    S right_{};
};

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
        return S{}.load_unaligned(p);
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
        return S{}.load_aligned(p);
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
inline S sqrt(const S& a) noexcept {
    if constexpr (SimdBatch<S>)
        return xsimd::sqrt(a);
    else
        return std::sqrt(a);
}

template <typename T>
inline T select(const mask_t<T>& mask, const T& true_val, const T& false_val) {
    if constexpr (SimdBatch<T>)
        return xsimd::select(mask, true_val, false_val);
    else
        return mask ? true_val : false_val;
}
}  // namespace applause
