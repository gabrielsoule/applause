#pragma once

#include <applause/util/SampleType.h>
#include <applause/util/DebugHelpers.h>
#include <complex>

namespace applause {

enum class StateVariableFilterType {
    Lowpass,
    Bandpass,
    Highpass,
};

/**
 * A second order digital state variable filter, digitized via trapezoidal integration.
 * This filter is stable under rapid parameter
 * modulation, unlike the second order biquad, and it is stable for all valid
 * cutoff and resonance values, unlike the Chamberlin digital SVF.
 *
 * @tparam S The sample type to be used in the filter (scalar or SIMD batch)
 * @tparam Type The filter type (low, band, high, etc)
 * @tparam UnityGain If true, normalizes output to prevent gain boost at resonance
 */
template <Sample S, StateVariableFilterType Type, bool UnityGain = false>
class StateVariableFilter {
public:
    using SampleType = S;
    using ScalarType = scalar_t<S>;

    static constexpr StateVariableFilterType filter_type = Type;

    StateVariableFilter() {
        reset();
    }

    void init(double sample_rate) {
        sample_rate_ = sample_rate;
        nyquist_limit_ = static_cast<ScalarType>(sample_rate * 0.4999);
    }

    void reset() {
        s1_ = s2_ = SampleType(0.0);
    }

    template <bool should_update = true>
    void setCutoffFrequency(SampleType frequency) {
        if constexpr (SimdBatch<SampleType>) {
            ASSERT(xsimd::all(frequency < SampleType(nyquist_limit_)),
                   "Frequency exceeds Nyquist");
        } else {
            ASSERT(frequency < nyquist_limit_,
                   "Frequency exceeds Nyquist");
        }

        cutoff_ = frequency;
        const ScalarType pi_over_sr = ScalarType(M_PI) / ScalarType(sample_rate_);

        using std::tan;
        using xsimd::tan;
        g_ = tan(cutoff_ * pi_over_sr);
        if constexpr (should_update)
            update();
    }

    template <bool should_update = true>
    void setQValue(SampleType q) {
        if constexpr (SimdBatch<SampleType>) {
            ASSERT(xsimd::all(q > SampleType(0.0)), "Q must be positive");
        } else {
            ASSERT(q > SampleType(0.0), "Q must be positive");
        }
        q_ = q;
        k_ = static_cast<SampleType>(1.0) / q;
        if constexpr (should_update)
            update();
    }

    /**
     * Returns the peak gain of the filter (maximum amplitude response).
     * For LP/HP filters, resonance creates a peak only when Q > 1/sqrt(2).
     * For BP filters, peak gain equals Q.
     */
    [[nodiscard]] SampleType getPeakGain() const noexcept {
        constexpr ScalarType inv_sqrt_two = ScalarType(0.70710678118654752440);

        if constexpr (filter_type == StateVariableFilterType::Lowpass ||
                      filter_type == StateVariableFilterType::Highpass) {
            using std::sqrt;
            using xsimd::sqrt;
            if constexpr (SimdBatch<SampleType>) {
                const auto has_resonance = q_ > SampleType(inv_sqrt_two);
                const auto k2 = k_ * k_;
                const auto peak = SampleType(2.0) / (k2 * sqrt(SampleType(4.0) / k2 - SampleType(1.0)));
                return xsimd::select(has_resonance, peak, SampleType(1.0));
            } else {
                if (q_ > inv_sqrt_two) {
                    const auto k2 = k_ * k_;
                    return ScalarType(2.0) / (k2 * sqrt(ScalarType(4.0) / k2 - ScalarType(1.0)));
                }
                return SampleType(1.0);
            }
        } else if constexpr (filter_type == StateVariableFilterType::Bandpass) {
            return q_;
        } else {
            return SampleType(1.0);
        }
    }

    [[nodiscard]] SampleType getCutoffFrequency() const noexcept {
        return cutoff_;
    }

    [[nodiscard]] SampleType getResonance() const noexcept {
        return q_;
    }

    [[nodiscard]] SampleType getPeakFrequency() const noexcept {
        constexpr ScalarType inv_sqrt_two = ScalarType(0.70710678118654752440);

        if constexpr (filter_type == StateVariableFilterType::Bandpass) {
            return cutoff_;
        }

        using std::sqrt;
        using xsimd::sqrt;

        if constexpr (SimdBatch<SampleType>) {
            const auto has_peak = q_ > SampleType(inv_sqrt_two);
            const auto q2 = q_ * q_;
            const auto factor = sqrt(SampleType(1.0) - SampleType(0.5) / q2);
            return xsimd::select(has_peak, cutoff_ * factor, cutoff_);
        } else {
            if (q_ > inv_sqrt_two) {
                const auto q2 = q_ * q_;
                return cutoff_ * sqrt(ScalarType(1.0) - ScalarType(0.5) / q2);
            }
            return cutoff_;
        }
    }

    /**
     * Sets the filter frequency based on peak frequency rather than cutoff,
     * i.e. a cutoff is calculated such that its peak frequency, with respect to the current
     * resonance value, is equal to this function's input.
     */
    template <bool should_update = true>
    void setPeakFrequency(SampleType frequency) {
        if constexpr (SimdBatch<SampleType>) {
            ASSERT(xsimd::all(frequency < SampleType(nyquist_limit_)),
                   "Frequency exceeds Nyquist");
            ASSERT(xsimd::all(q_ > SampleType(ScalarType(0.70710678118654752440))),
                   "Q must be > sqrt(0.5) for peak frequency mode");
        } else {
            ASSERT(frequency < nyquist_limit_,
                   "Frequency exceeds Nyquist");
            ASSERT(q_ > ScalarType(0.70710678118654752440),
                   "Q must be > sqrt(0.5) for peak frequency mode");
        }

        using std::sqrt;
        using xsimd::sqrt;
        const auto q2 = q_ * q_;
        const auto factor = sqrt(SampleType(1.0) - SampleType(0.5) / q2);
        cutoff_ = frequency / factor;

        // Clamp cutoff to below Nyquist
        if constexpr (SimdBatch<SampleType>) {
            cutoff_ = xsimd::min(cutoff_, SampleType(nyquist_limit_));
        } else {
            cutoff_ = std::min(cutoff_, nyquist_limit_);
        }

        const ScalarType pi_over_sr = ScalarType(M_PI) / ScalarType(sample_rate_);

        using std::tan;
        using xsimd::tan;
        g_ = tan(cutoff_ * pi_over_sr);

        if constexpr (should_update)
            update();
    }

    void update() {
        gk_  = g_ + k_;
        d_ = static_cast<ScalarType>(1.0) / (static_cast<ScalarType>(1.0) + g_ * gk_);

        if constexpr (UnityGain) {
            one_over_peak_gain_ = SampleType(1.0) / getPeakGain();
        }
    }

    [[nodiscard]] SampleType processSample(SampleType input) noexcept {
        return processSampleInternal(input);
    }

    void process(const S* input, S* output, size_t num_frames) noexcept {
        for(size_t i=0; i<num_frames; ++i) {
            output[i] = processSampleInternal(input[i]);
        }
    }

    /**
     * Returns the phase delay of the filter at the given frequency, in samples.
     *
     * This function currently doesn't implement SIMD acceleration, even
     * if the filter is templated with respect to a SIMD sample type.
     *
     */
    [[nodiscard]] SampleType getPhaseDelayInSamples(SampleType frequency) const noexcept {
        if constexpr (SimdBatch<SampleType>) {
            alignas(64) ScalarType freq_arr[SampleType::size];
            alignas(64) ScalarType g_arr[SampleType::size];
            alignas(64) ScalarType k_arr[SampleType::size];
            alignas(64) ScalarType result_arr[SampleType::size];

            frequency.store_aligned(freq_arr);
            g_.store_aligned(g_arr);
            k_.store_aligned(k_arr);

            for (size_t i = 0; i < SampleType::size; ++i) {
                result_arr[i] = computePhaseDelayScalar(freq_arr[i], g_arr[i], k_arr[i]);
            }

            return SampleType::load_aligned(result_arr);
        } else {
            return computePhaseDelayScalar(frequency, g_, k_);
        }
    }

private:
    [[nodiscard]] __attribute__((always_inline)) SampleType processSampleInternal(SampleType x) noexcept {
        const auto yhp = (x - gk_ * s1_ - s2_) * d_;

        const auto v1 = g_ * yhp;
        const auto ybp = v1 + s1_;
        s1_ = ybp + v1;

        const auto v2 = g_ * ybp;
        const auto ylp = v2 + s2_;
        s2_ = ylp + v2;

        SampleType output;
        if constexpr (filter_type == StateVariableFilterType::Lowpass) {
            output = ylp;
        } else if constexpr (filter_type == StateVariableFilterType::Bandpass) {
            output = ybp;
        } else if constexpr (filter_type == StateVariableFilterType::Highpass) {
            output = yhp;
        } else {
            LOG_ERR("Unknown filter type; this should never happen!");
            return SampleType(0);
        }

        if constexpr (UnityGain) {
            return output * one_over_peak_gain_;
        } else {
            return output;
        }
    }

    /**
     * Computes phase delay for a single set of scalar parameters using complex arithmetic.
     * This evaluates the filter's z-domain transfer function at the given frequency.
     */
    [[nodiscard]] ScalarType computePhaseDelayScalar(ScalarType freq, ScalarType g, ScalarType k) const noexcept {
        if (freq <= ScalarType(0))
            return ScalarType(0);

        const ScalarType omega = ScalarType(2.0 * M_PI) * freq / ScalarType(sample_rate_);
        const std::complex<ScalarType> j(0, 1);
        const std::complex<ScalarType> z = std::exp(j * omega);

        const auto g2 = g * g;
        const auto gk = g * k;

        std::complex<ScalarType> num;
        if constexpr (filter_type == StateVariableFilterType::Lowpass) {
            const auto one_plus_z = ScalarType(1) + z;
            num = g2 * one_plus_z * one_plus_z;
        } else if constexpr (filter_type == StateVariableFilterType::Bandpass) {
            num = g * (z * z - ScalarType(1));
        } else if constexpr (filter_type == StateVariableFilterType::Highpass) {
            const auto z_minus_one = z - ScalarType(1);
            num = z_minus_one * z_minus_one;
        }

        const auto z_minus_one = z - ScalarType(1);
        const auto z_plus_one = z + ScalarType(1);
        const auto den = z_minus_one * z_minus_one
                       + g2 * z_plus_one * z_plus_one
                       + gk * (z * z - ScalarType(1));

        const auto H = num / den;
        const auto phase = std::arg(H);

        return -phase / omega;
    }

    SampleType cutoff_;
    SampleType q_;
    SampleType k_;

    SampleType g_;
    SampleType gk_;
    SampleType d_;

    SampleType s1_;
    SampleType s2_;
    SampleType one_over_peak_gain_ = SampleType(1.0);

    double sample_rate_ = -1;
    ScalarType nyquist_limit_ = -1;
};

template <Sample S = float>
using SVFLowpass = StateVariableFilter<S, StateVariableFilterType::Lowpass, false>;

template <Sample S = float>
using SVFHighpass = StateVariableFilter<S, StateVariableFilterType::Highpass, false>;

template <Sample S = float>
using SVFBandpass = StateVariableFilter<S, StateVariableFilterType::Bandpass, false>;

} // namespace applause