#include <applause/dsp/filters/StateVariableFilter.h>
#include <applause/util/SampleType.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <numbers>
#include <type_traits>

using Catch::Approx;

namespace {

constexpr double sample_rate = 48000.0;
constexpr double default_cutoff = 1000.0;
constexpr double default_q = 2.0;

template <typename Scalar>
std::array<Scalar, 256> testSignal() {
    std::array<Scalar, 256> signal{};
    signal[0] = Scalar(1.0);
    uint32_t state = 0x12345678;
    for (size_t i = 1; i < signal.size(); ++i) {
        state = state * 1664525u + 1013904223u;
        signal[i] = Scalar(state >> 8) / Scalar(1u << 24) - Scalar(0.5);
    }
    return signal;
}

template <applause::Sample S>
double laneValue(const S& value, size_t lane = 0) {
    if constexpr (applause::SimdBatch<S>)
        return static_cast<double>(value.get(lane));
    else
        return static_cast<double>(value);
}

template <applause::Sample S>
void requireClose(const S& actual, const S& expected, double tolerance) {
    for (size_t lane = 0; lane < applause::sample_width_v<S>; ++lane) {
        CAPTURE(lane);
        REQUIRE(laneValue(actual, lane) == Approx(laneValue(expected, lane)).epsilon(0.0).margin(tolerance));
    }
}

template <typename Filter>
Filter configureFilter(Filter filter, double rate, typename Filter::SampleType cutoff, typename Filter::SampleType q) {
    filter.init(rate);
    filter.setCutoffFrequency(cutoff);
    filter.setQValue(q);
    return filter;
}

struct ExpectedTaps {
    std::complex<double> lp;
    std::complex<double> bp;
    std::complex<double> hp;
};

// Independent bilinear-domain response, with ratio = tan(pi f / fs) / tan(pi fc / fs).
ExpectedTaps expectedTaps(double frequency, double cutoff, double q, double rate) {
    const double ratio = std::tan(std::numbers::pi * frequency / rate) / std::tan(std::numbers::pi * cutoff / rate);
    const std::complex<double> denominator{1.0 - ratio * ratio, ratio / q};
    return {
        1.0 / denominator,
        std::complex<double>{0.0, ratio} / denominator,
        (-ratio * ratio) / denominator,
    };
}

template <applause::Sample S, size_t NumFrequencies, typename Filter>
auto measuredResponses(Filter& filter, const std::array<double, NumFrequencies>& frequencies, double rate) {
    // Evaluate the processed impulse's DTFT directly at each requested frequency.
    constexpr size_t impulse_length = 1024;
    using LaneResponse = std::array<std::complex<double>, applause::sample_width_v<S>>;
    std::array<LaneResponse, NumFrequencies> responses{};
    std::array<std::complex<double>, NumFrequencies> phase{};
    std::array<std::complex<double>, NumFrequencies> step{};

    for (size_t i = 0; i < NumFrequencies; ++i) {
        phase[i] = 1.0;
        step[i] = std::polar(1.0, -2.0 * std::numbers::pi * frequencies[i] / rate);
    }

    filter.reset();
    for (size_t n = 0; n < impulse_length; ++n) {
        const auto input = applause::set1<S>(applause::scalar_t<S>(n == 0));
        const S output = filter.processSample(input);
        for (size_t i = 0; i < NumFrequencies; ++i) {
            for (size_t lane = 0; lane < applause::sample_width_v<S>; ++lane)
                responses[i][lane] += laneValue(output, lane) * phase[i];
            phase[i] *= step[i];
        }
    }

    return responses;
}

template <applause::Sample S, size_t NumFrequencies, typename Filter, typename ExpectedResponse>
void requireTransferResponse(Filter filter, const std::array<double, NumFrequencies>& frequencies, double rate,
                             ExpectedResponse expectedResponse) {
    using Scalar = applause::scalar_t<S>;
    const double response_tolerance = std::is_same_v<Scalar, float> ? 2e-5 : 2e-10;
    const double delay_tolerance = std::is_same_v<Scalar, float> ? 2e-4 : 2e-9;
    const auto measured = measuredResponses<S>(filter, frequencies, rate);

    for (size_t i = 0; i < frequencies.size(); ++i) {
        const auto expected = expectedResponse(frequencies[i]);
        const auto frequency = applause::set1<S>(Scalar(frequencies[i]));
        const S reported_delay = filter.getPhaseDelayInSamples(frequency);
        const double omega = 2.0 * std::numbers::pi * frequencies[i] / rate;

        for (size_t lane = 0; lane < applause::sample_width_v<S>; ++lane) {
            CAPTURE(frequencies[i], lane);
            REQUIRE(measured[i][lane].real() == Approx(expected.real()).epsilon(0.0).margin(response_tolerance));
            REQUIRE(measured[i][lane].imag() == Approx(expected.imag()).epsilon(0.0).margin(response_tolerance));
            const double measured_delay = -std::arg(measured[i][lane]) / omega;
            REQUIRE(laneValue(reported_delay, lane) == Approx(measured_delay).epsilon(0.0).margin(delay_tolerance));
        }
    }
}

template <typename Filter>
concept HasSetMode = requires(Filter& filter, typename Filter::SampleType value) { filter.setMode(value); };

template <typename Filter>
concept HasPeakGain = requires(const Filter& filter) { filter.getPeakGain(); };

template <typename Filter>
concept HasPeakFrequency = requires(const Filter& filter) { filter.getPeakFrequency(); };

template <typename Filter>
concept HasSetPeakFrequency =
    requires(Filter& filter, typename Filter::SampleType value) { filter.setPeakFrequency(value); };

// MultiMode + UnityGain remains an intentional class-level compile error.
static_assert(HasSetMode<applause::SVFMultiMode<float>>);
static_assert(!HasSetMode<applause::SVFLowpass<float>>);
static_assert(!HasPeakGain<applause::SVFMultiMode<float>>);
static_assert(!HasPeakFrequency<applause::SVFMultiMode<float>>);
static_assert(!HasSetPeakFrequency<applause::SVFMultiMode<float>>);
static_assert(HasPeakGain<applause::SVFLowpass<float>>);
static_assert(HasPeakFrequency<applause::SVFLowpass<float>>);
static_assert(HasSetPeakFrequency<applause::SVFLowpass<float>>);

}  // namespace

TEMPLATE_TEST_CASE("StateVariableFilter matches the expected transfer response and phase", "[dsp][filter][svf][simd]",
                   float, double, xsimd::batch<float>, xsimd::batch<double>) {
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    constexpr std::array frequencies{250.0, 1000.0, 4000.0};
    const auto cutoff = applause::set1<S>(Scalar(default_cutoff));
    const auto q = applause::set1<S>(Scalar(default_q));

    SECTION("lowpass") {
        auto filter = configureFilter(applause::SVFLowpass<S>{}, sample_rate, cutoff, q);
        requireTransferResponse<S>(filter, frequencies, sample_rate, [](double frequency) {
            return expectedTaps(frequency, default_cutoff, default_q, sample_rate).lp;
        });
    }

    SECTION("bandpass") {
        auto filter = configureFilter(applause::SVFBandpass<S>{}, sample_rate, cutoff, q);
        requireTransferResponse<S>(filter, frequencies, sample_rate, [](double frequency) {
            return expectedTaps(frequency, default_cutoff, default_q, sample_rate).bp;
        });
    }

    SECTION("highpass") {
        auto filter = configureFilter(applause::SVFHighpass<S>{}, sample_rate, cutoff, q);
        requireTransferResponse<S>(filter, frequencies, sample_rate, [](double frequency) {
            return expectedTaps(frequency, default_cutoff, default_q, sample_rate).hp;
        });
    }

    SECTION("MultiMode lowpass-bandpass blend") {
        constexpr double inv_sqrt_two = 0.70710678118654752440;
        auto filter = configureFilter(applause::SVFMultiMode<S>{}, sample_rate, cutoff, q);
        filter.setMode(applause::set1<S>(Scalar(0.25)));
        requireTransferResponse<S>(filter, frequencies, sample_rate, [=](double frequency) {
            const auto taps = expectedTaps(frequency, default_cutoff, default_q, sample_rate);
            return inv_sqrt_two * taps.lp + taps.bp;
        });
    }

    SECTION("MultiMode bandpass-highpass blend") {
        constexpr double inv_sqrt_two = 0.70710678118654752440;
        auto filter = configureFilter(applause::SVFMultiMode<S>{}, sample_rate, cutoff, q);
        filter.setMode(applause::set1<S>(Scalar(0.75)));
        requireTransferResponse<S>(filter, frequencies, sample_rate, [=](double frequency) {
            const auto taps = expectedTaps(frequency, default_cutoff, default_q, sample_rate);
            return taps.bp + inv_sqrt_two * taps.hp;
        });
    }
}

TEMPLATE_TEST_CASE("StateVariableFilter MultiMode selects and blends the discrete responses",
                   "[dsp][filter][svf][simd]", float, double, xsimd::batch<float>, xsimd::batch<double>) {
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    const auto cutoff = applause::set1<S>(Scalar(default_cutoff));
    const auto q = applause::set1<S>(Scalar(default_q));
    const auto signal = testSignal<Scalar>();
    const double tolerance = std::is_same_v<Scalar, float> ? 1e-5 : 1e-12;

    auto makeFilter = [&](auto filter) { return configureFilter(filter, sample_rate, cutoff, q); };

    auto requireSameOutput = [&](auto& multi_mode, auto&& reference) {
        for (Scalar x : signal)
            requireClose(multi_mode.processSample(applause::set1<S>(x)), reference(applause::set1<S>(x)), tolerance);
    };

    SECTION("the default mode is lowpass") {
        auto multi_mode = makeFilter(applause::SVFMultiMode<S>{});
        auto lp = makeFilter(applause::SVFLowpass<S>{});
        requireSameOutput(multi_mode, [&](S x) { return lp.processSample(x); });
    }

    SECTION("mode 0 is lowpass") {
        auto multi_mode = makeFilter(applause::SVFMultiMode<S>{});
        auto lp = makeFilter(applause::SVFLowpass<S>{});
        multi_mode.setMode(applause::set1<S>(Scalar(0.0)));
        requireSameOutput(multi_mode, [&](S x) { return lp.processSample(x); });
    }

    SECTION("mode 0.5 is bandpass with sqrt(2) compensation") {
        auto multi_mode = makeFilter(applause::SVFMultiMode<S>{});
        auto bp = makeFilter(applause::SVFBandpass<S>{});
        multi_mode.setMode(applause::set1<S>(Scalar(0.5)));
        const auto sqrt_two = applause::set1<S>(Scalar(std::numbers::sqrt2));
        requireSameOutput(multi_mode, [&](S x) { return bp.processSample(x) * sqrt_two; });
    }

    SECTION("mode 1 is highpass") {
        auto multi_mode = makeFilter(applause::SVFMultiMode<S>{});
        auto hp = makeFilter(applause::SVFHighpass<S>{});
        multi_mode.setMode(applause::set1<S>(Scalar(1.0)));
        requireSameOutput(multi_mode, [&](S x) { return hp.processSample(x); });
    }

    SECTION("mode 0.25 blends lowpass and bandpass") {
        auto multi_mode = makeFilter(applause::SVFMultiMode<S>{});
        auto lp = makeFilter(applause::SVFLowpass<S>{});
        auto bp = makeFilter(applause::SVFBandpass<S>{});
        const auto inv_sqrt_two = applause::set1<S>(Scalar(1.0 / std::numbers::sqrt2));
        multi_mode.setMode(applause::set1<S>(Scalar(0.25)));
        requireSameOutput(multi_mode, [&](S x) { return inv_sqrt_two * lp.processSample(x) + bp.processSample(x); });
    }

    SECTION("mode 0.75 blends bandpass and highpass") {
        auto multi_mode = makeFilter(applause::SVFMultiMode<S>{});
        auto bp = makeFilter(applause::SVFBandpass<S>{});
        auto hp = makeFilter(applause::SVFHighpass<S>{});
        const auto inv_sqrt_two = applause::set1<S>(Scalar(1.0 / std::numbers::sqrt2));
        multi_mode.setMode(applause::set1<S>(Scalar(0.75)));
        requireSameOutput(multi_mode, [&](S x) { return bp.processSample(x) + inv_sqrt_two * hp.processSample(x); });
    }

    SECTION("phase delay matches all three discrete response nodes") {
        auto multi_mode = makeFilter(applause::SVFMultiMode<S>{});
        auto lp = makeFilter(applause::SVFLowpass<S>{});
        auto bp = makeFilter(applause::SVFBandpass<S>{});
        auto hp = makeFilter(applause::SVFHighpass<S>{});
        const auto frequency = applause::set1<S>(Scalar(500.0));

        multi_mode.setMode(applause::set1<S>(Scalar(0.0)));
        requireClose(multi_mode.getPhaseDelayInSamples(frequency), lp.getPhaseDelayInSamples(frequency), tolerance);
        multi_mode.setMode(applause::set1<S>(Scalar(0.5)));
        requireClose(multi_mode.getPhaseDelayInSamples(frequency), bp.getPhaseDelayInSamples(frequency), tolerance);
        multi_mode.setMode(applause::set1<S>(Scalar(1.0)));
        requireClose(multi_mode.getPhaseDelayInSamples(frequency), hp.getPhaseDelayInSamples(frequency), tolerance);
    }
}

TEMPLATE_TEST_CASE("StateVariableFilter MultiMode changes mode without changing state", "[dsp][filter][svf][simd]",
                   float, double, xsimd::batch<float>, xsimd::batch<double>) {
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    const auto cutoff = applause::set1<S>(Scalar(default_cutoff));
    const auto q = applause::set1<S>(Scalar(default_q));
    const auto signal = testSignal<Scalar>();
    const auto inv_sqrt_two = applause::set1<S>(Scalar(1.0 / std::numbers::sqrt2));
    const double tolerance = std::is_same_v<Scalar, float> ? 1e-5 : 1e-12;

    auto multi_mode = configureFilter(applause::SVFMultiMode<S>{}, sample_rate, cutoff, q);
    auto lp = configureFilter(applause::SVFLowpass<S>{}, sample_rate, cutoff, q);
    auto bp = configureFilter(applause::SVFBandpass<S>{}, sample_rate, cutoff, q);
    auto hp = configureFilter(applause::SVFHighpass<S>{}, sample_rate, cutoff, q);

    for (size_t i = 0; i < 64; ++i) {
        const auto input = applause::set1<S>(signal[i]);
        static_cast<void>(multi_mode.processSample(input));
        static_cast<void>(lp.processSample(input));
        static_cast<void>(bp.processSample(input));
        static_cast<void>(hp.processSample(input));
    }

    multi_mode.setMode(applause::set1<S>(Scalar(0.75)));
    for (size_t i = 64; i < 160; ++i) {
        const auto input = applause::set1<S>(signal[i]);
        const S actual = multi_mode.processSample(input);
        static_cast<void>(lp.processSample(input));
        const S bp_output = bp.processSample(input);
        const S hp_output = hp.processSample(input);
        requireClose(actual, bp_output + inv_sqrt_two * hp_output, tolerance);
    }

    multi_mode.setMode(applause::set1<S>(Scalar(0.25)));
    for (size_t i = 160; i < signal.size(); ++i) {
        const auto input = applause::set1<S>(signal[i]);
        const S actual = multi_mode.processSample(input);
        const S lp_output = lp.processSample(input);
        const S bp_output = bp.processSample(input);
        static_cast<void>(hp.processSample(input));
        requireClose(actual, inv_sqrt_two * lp_output + bp_output, tolerance);
    }
}

TEMPLATE_TEST_CASE("StateVariableFilter SIMD lanes support independent rapid modulation", "[dsp][filter][svf][simd]",
                   xsimd::batch<float>, xsimd::batch<double>) {
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    const double output_tolerance = std::is_same_v<Scalar, float> ? 3e-5 : 1e-11;
    const double phase_tolerance = std::is_same_v<Scalar, float> ? 5e-4 : 1e-9;

    auto simd_filter = configureFilter(applause::SVFMultiMode<S>{}, sample_rate,
                                       applause::set1<S>(Scalar(default_cutoff)), applause::set1<S>(Scalar(default_q)));
    std::array<applause::SVFMultiMode<Scalar>, S::size> scalar_filters;
    for (auto& filter : scalar_filters) {
        filter = configureFilter(filter, sample_rate, Scalar(default_cutoff), Scalar(default_q));
    }

    alignas(64) std::array<Scalar, S::size> cutoffs{};
    alignas(64) std::array<Scalar, S::size> qs{};
    alignas(64) std::array<Scalar, S::size> modes{};
    alignas(64) std::array<Scalar, S::size> inputs{};
    for (size_t sample = 0; sample < 128; ++sample) {
        for (size_t lane = 0; lane < S::size; ++lane) {
            cutoffs[lane] = Scalar(300 + (sample * 37 + lane * 211) % 6000);
            qs[lane] = Scalar(0.55 + 0.1 * double((sample + lane * 3) % 15));
            modes[lane] = Scalar(double((sample * 3 + lane * 5) % 17) / 16.0);
            inputs[lane] = Scalar(std::sin(0.031 * double(sample + lane * 13)));

            scalar_filters[lane].setCutoffFrequency(cutoffs[lane]);
            scalar_filters[lane].setQValue(qs[lane]);
            scalar_filters[lane].setMode(modes[lane]);
        }

        simd_filter.setCutoffFrequency(S::load_aligned(cutoffs.data()));
        simd_filter.setQValue(S::load_aligned(qs.data()));
        simd_filter.setMode(S::load_aligned(modes.data()));
        const S output = simd_filter.processSample(S::load_aligned(inputs.data()));

        for (size_t lane = 0; lane < S::size; ++lane) {
            CAPTURE(sample, lane);
            const Scalar expected = scalar_filters[lane].processSample(inputs[lane]);
            REQUIRE(std::isfinite(output.get(lane)));
            REQUIRE(output.get(lane) == Approx(expected).epsilon(0.0).margin(output_tolerance));
        }
    }

    alignas(64) std::array<Scalar, S::size> frequencies{};
    for (size_t lane = 0; lane < S::size; ++lane) frequencies[lane] = Scalar(500 + lane * 275);

    const S phase_delay = simd_filter.getPhaseDelayInSamples(S::load_aligned(frequencies.data()));
    for (size_t lane = 0; lane < S::size; ++lane) {
        CAPTURE(lane);
        const Scalar expected = scalar_filters[lane].getPhaseDelayInSamples(frequencies[lane]);
        REQUIRE(phase_delay.get(lane) == Approx(expected).epsilon(0.0).margin(phase_tolerance));
    }
}

TEMPLATE_TEST_CASE("StateVariableFilter block processing, reset, and reinitialization preserve semantics",
                   "[dsp][filter][svf][simd]", float, double, xsimd::batch<float>, xsimd::batch<double>) {
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    const auto cutoff = applause::set1<S>(Scalar(default_cutoff));
    const auto q = applause::set1<S>(Scalar(default_q));
    const auto mode = applause::set1<S>(Scalar(0.75));
    const auto signal = testSignal<Scalar>();
    const double tolerance = std::is_same_v<Scalar, float> ? 1e-5 : 1e-12;

    auto base = configureFilter(applause::SVFMultiMode<S>{}, sample_rate, cutoff, q);
    base.setMode(mode);

    std::array<S, 256> input{};
    for (size_t i = 0; i < input.size(); ++i) input[i] = applause::set1<S>(signal[i]);

    SECTION("init establishes usable conventional defaults") {
        applause::SVFLowpass<S> filter;
        filter.init(sample_rate);
        requireClose(filter.getCutoffFrequency(), applause::set1<S>(Scalar(1000.0)), 0.0);
        requireClose(filter.getResonance(), applause::set1<S>(Scalar(0.70710678118654752440)), 0.0);
        const S output = filter.processSample(applause::set1<S>(Scalar(1.0)));
        for (size_t lane = 0; lane < applause::sample_width_v<S>; ++lane)
            REQUIRE(std::isfinite(laneValue(output, lane)));
    }

    SECTION("block and in-place processing match processSample") {
        auto sample_filter = base;
        auto block_filter = base;
        auto in_place_filter = base;
        std::array<S, 256> expected{};
        std::array<S, 256> block_output{};
        for (size_t i = 0; i < input.size(); ++i) expected[i] = sample_filter.processSample(input[i]);

        block_filter.process(input.data(), block_output.data(), input.size());
        auto in_place_output = input;
        in_place_filter.process(in_place_output.data(), in_place_output.data(), in_place_output.size());
        for (size_t i = 0; i < input.size(); ++i) {
            CAPTURE(i);
            requireClose(block_output[i], expected[i], tolerance);
            requireClose(in_place_output[i], expected[i], tolerance);
        }
    }

    SECTION("reset clears state and preserves parameters and mode") {
        auto reset_filter = base;
        for (const S& value : input) static_cast<void>(reset_filter.processSample(value));
        reset_filter.reset();
        auto fresh_filter = base;
        for (const S& value : input)
            requireClose(reset_filter.processSample(value), fresh_filter.processSample(value), tolerance);
        requireClose(reset_filter.getCutoffFrequency(), cutoff, 0.0);
        requireClose(reset_filter.getResonance(), q, 0.0);
    }

    SECTION("reinitialization recomputes coefficients and clears state") {
        auto reinitialized = base;
        for (const S& value : input) static_cast<void>(reinitialized.processSample(value));
        constexpr double new_sample_rate = 96000.0;
        reinitialized.init(new_sample_rate);
        auto fresh_at_new_rate = configureFilter(applause::SVFMultiMode<S>{}, new_sample_rate, cutoff, q);
        fresh_at_new_rate.setMode(mode);
        for (const S& value : input)
            requireClose(reinitialized.processSample(value), fresh_at_new_rate.processSample(value), tolerance);
    }

    SECTION("a zero-length block leaves the filter untouched") {
        auto filter = base;
        auto reference = base;
        for (size_t i = 0; i < 16; ++i) {
            static_cast<void>(filter.processSample(input[i]));
            static_cast<void>(reference.processSample(input[i]));
        }
        std::array<S, 1> unused{};
        filter.process(unused.data(), unused.data(), 0);
        filter.process(1, unused.data(), unused.data(), 0);
        for (size_t i = 16; i < 48; ++i)
            requireClose(filter.processSample(input[i]), reference.processSample(input[i]), 0.0);
    }

    SECTION("reinitialization clamps a cutoff above the new Nyquist limit") {
        const auto high_cutoff = applause::set1<S>(Scalar(18000.0));
        auto reinitialized = configureFilter(applause::SVFMultiMode<S>{}, sample_rate, high_cutoff, q);
        reinitialized.setMode(mode);
        constexpr double lower_sample_rate = 32000.0;
        reinitialized.init(lower_sample_rate);
        const S clamped_cutoff = reinitialized.getCutoffFrequency();
        for (size_t lane = 0; lane < applause::sample_width_v<S>; ++lane) {
            REQUIRE(laneValue(clamped_cutoff, lane) >= 0.0);
            REQUIRE(laneValue(clamped_cutoff, lane) < lower_sample_rate * 0.4999);
        }

        auto fresh_at_lower_rate = configureFilter(applause::SVFMultiMode<S>{}, lower_sample_rate, clamped_cutoff, q);
        fresh_at_lower_rate.setMode(mode);
        for (const S& value : input)
            requireClose(reinitialized.processSample(value), fresh_at_lower_rate.processSample(value), tolerance);
    }
}

TEMPLATE_TEST_CASE("StateVariableFilter eager and deferred coefficient updates agree", "[dsp][filter][svf][simd]",
                   float, double, xsimd::batch<float>, xsimd::batch<double>) {
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    const auto initial_cutoff = applause::set1<S>(Scalar(700.0));
    const auto initial_q = applause::set1<S>(Scalar(0.8));
    const auto new_cutoff = applause::set1<S>(Scalar(2300.0));
    const auto new_q = applause::set1<S>(Scalar(1.6));
    const auto signal = testSignal<Scalar>();
    const double tolerance = std::is_same_v<Scalar, float> ? 1e-5 : 1e-12;

    auto eager = configureFilter(applause::SVFLowpass<S>{}, sample_rate, initial_cutoff, initial_q);
    auto deferred = eager;
    eager.setCutoffFrequency(new_cutoff);
    eager.setQValue(new_q);
    deferred.template setCutoffFrequency<false>(new_cutoff);
    deferred.template setQValue<false>(new_q);
    deferred.update();

    requireClose(eager.getCutoffFrequency(), new_cutoff, 0.0);
    requireClose(eager.getResonance(), new_q, 0.0);
    for (Scalar value : signal) {
        const auto input = applause::set1<S>(value);
        requireClose(eager.processSample(input), deferred.processSample(input), tolerance);
    }

    // Same agreement for the peak-frequency path (requires Q > sqrt(0.5)).
    auto eager_peak = configureFilter(applause::SVFLowpass<S>{}, sample_rate, initial_cutoff, new_q);
    auto deferred_peak = eager_peak;
    const auto peak_target = applause::set1<S>(Scalar(1800.0));
    eager_peak.setPeakFrequency(peak_target);
    deferred_peak.template setPeakFrequency<false>(peak_target);
    deferred_peak.update();
    for (Scalar value : signal) {
        const auto input = applause::set1<S>(value);
        requireClose(eager_peak.processSample(input), deferred_peak.processSample(input), tolerance);
    }
}

TEMPLATE_TEST_CASE("StateVariableFilter shares one coefficient set across independent channel states",
                   "[dsp][filter][svf][simd]", float, double, xsimd::batch<float>, xsimd::batch<double>) {
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    const auto cutoff = applause::set1<S>(Scalar(default_cutoff));
    const auto q = applause::set1<S>(Scalar(default_q));
    const auto signal = testSignal<Scalar>();
    const double tolerance = std::is_same_v<Scalar, float> ? 1e-5 : 1e-12;

    static_assert(applause::SVFLowpass<S>::max_channel_count == 2);
    static_assert(applause::SVFLowpass<S, 4>::max_channel_count == 4);

    SECTION("each channel filters its own signal as if it were a standalone filter") {
        auto stereo = configureFilter(applause::SVFLowpass<S>{}, sample_rate, cutoff, q);
        auto left_reference = configureFilter(applause::SVFLowpass<S>{}, sample_rate, cutoff, q);
        auto right_reference = configureFilter(applause::SVFLowpass<S>{}, sample_rate, cutoff, q);

        for (size_t i = 0; i < signal.size(); ++i) {
            const auto left_input = applause::set1<S>(signal[i]);
            const auto right_input = applause::set1<S>(-signal[signal.size() - 1 - i]);
            CAPTURE(i);
            requireClose(stereo.processSample(0, left_input), left_reference.processSample(left_input), tolerance);
            requireClose(stereo.processSample(1, right_input), right_reference.processSample(right_input), tolerance);
        }
    }

    SECTION("the overloads without a channel index process channel 0") {
        auto indexed = configureFilter(applause::SVFBandpass<S>{}, sample_rate, cutoff, q);
        auto implicit = indexed;
        for (Scalar value : signal) {
            const auto input = applause::set1<S>(value);
            requireClose(indexed.processSample(0, input), implicit.processSample(input), 0.0);
        }
    }

    SECTION("per-channel block processing matches per-sample processing") {
        auto block_filter = configureFilter(applause::SVFHighpass<S>{}, sample_rate, cutoff, q);
        auto sample_filter = block_filter;

        std::array<S, 256> left_input{};
        std::array<S, 256> right_input{};
        for (size_t i = 0; i < left_input.size(); ++i) {
            left_input[i] = applause::set1<S>(signal[i]);
            right_input[i] = applause::set1<S>(-signal[signal.size() - 1 - i]);
        }

        std::array<S, 256> left_output{};
        std::array<S, 256> right_output{};
        block_filter.process(0, left_input.data(), left_output.data(), left_input.size());
        block_filter.process(1, right_input.data(), right_output.data(), right_input.size());

        for (size_t i = 0; i < left_input.size(); ++i) {
            CAPTURE(i);
            requireClose(left_output[i], sample_filter.processSample(0, left_input[i]), tolerance);
            requireClose(right_output[i], sample_filter.processSample(1, right_input[i]), tolerance);
        }
    }

    SECTION("reset clears every channel") {
        auto filter = configureFilter(applause::SVFMultiMode<S>{}, sample_rate, cutoff, q);
        auto fresh = filter;
        for (Scalar value : signal) {
            static_cast<void>(filter.processSample(0, applause::set1<S>(value)));
            static_cast<void>(filter.processSample(1, applause::set1<S>(-value)));
        }
        filter.reset();
        for (Scalar value : signal) {
            const auto input = applause::set1<S>(value);
            requireClose(filter.processSample(0, input), fresh.processSample(0, input), 0.0);
            requireClose(filter.processSample(1, input), fresh.processSample(1, input), 0.0);
        }
    }

    SECTION("channel counts other than stereo work") {
        constexpr size_t channels = 4;
        auto bank = configureFilter(applause::SVFLowpass<S, channels>{}, sample_rate, cutoff, q);
        std::array<applause::SVFLowpass<S>, channels> references;
        for (auto& reference : references)
            reference = configureFilter(reference, sample_rate, cutoff, q);

        for (size_t i = 0; i < signal.size(); ++i) {
            for (size_t channel = 0; channel < channels; ++channel) {
                const auto input = applause::set1<S>(signal[(i + channel * 17) % signal.size()]);
                CAPTURE(i, channel);
                requireClose(bank.processSample(channel, input), references[channel].processSample(input), tolerance);
            }
        }
    }
}

TEMPLATE_TEST_CASE("StateVariableFilter stays finite and bounded at extreme valid settings",
                   "[dsp][filter][svf][simd]", float, double, xsimd::batch<float>, xsimd::batch<double>) {
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    const auto signal = testSignal<Scalar>();

    // The class documents stability for all valid cutoff and resonance values,
    // including under rapid modulation; these sections push the extremes of
    // that claim and require only bounded, finite output.
    auto requireBoundedOutput = [&](auto& filter, double bound) {
        for (size_t repeat = 0; repeat < 4; ++repeat) {
            for (size_t i = 0; i < signal.size(); ++i) {
                const S output = filter.processSample(applause::set1<S>(signal[i]));
                for (size_t lane = 0; lane < applause::sample_width_v<S>; ++lane) {
                    CAPTURE(repeat, i, lane);
                    REQUIRE(std::isfinite(laneValue(output, lane)));
                    REQUIRE(std::abs(laneValue(output, lane)) <= bound);
                }
            }
        }
    };

    SECTION("cutoff just below the Nyquist limit") {
        auto filter = configureFilter(applause::SVFLowpass<S>{}, sample_rate,
                                      applause::set1<S>(Scalar(23990.0)),
                                      applause::set1<S>(Scalar(0.70710678)));
        requireBoundedOutput(filter, 100.0);
    }

    SECTION("subsonic cutoff") {
        auto filter = configureFilter(applause::SVFLowpass<S>{}, sample_rate,
                                      applause::set1<S>(Scalar(1.0)),
                                      applause::set1<S>(Scalar(0.70710678)));
        requireBoundedOutput(filter, 100.0);
    }

    SECTION("extreme resonance") {
        auto filter = configureFilter(applause::SVFBandpass<S>{}, sample_rate,
                                      applause::set1<S>(Scalar(default_cutoff)),
                                      applause::set1<S>(Scalar(100.0)));
        requireBoundedOutput(filter, 1e4);
    }

    SECTION("violent per-sample cutoff and mode modulation") {
        auto filter = configureFilter(applause::SVFMultiMode<S>{}, sample_rate,
                                      applause::set1<S>(Scalar(default_cutoff)),
                                      applause::set1<S>(Scalar(20.0)));
        for (size_t n = 0; n < 4 * signal.size(); ++n) {
            const Scalar modulated_cutoff = n % 2 == 0 ? Scalar(10.0) : Scalar(23990.0);
            filter.setCutoffFrequency(applause::set1<S>(modulated_cutoff));
            filter.setMode(applause::set1<S>(Scalar(double(n % 17) / 16.0)));
            const S output = filter.processSample(applause::set1<S>(signal[n % signal.size()]));
            for (size_t lane = 0; lane < applause::sample_width_v<S>; ++lane) {
                CAPTURE(n, lane);
                REQUIRE(std::isfinite(laneValue(output, lane)));
                REQUIRE(std::abs(laneValue(output, lane)) <= 1e4);
            }
        }
    }
}

TEMPLATE_TEST_CASE("StateVariableFilter peak helpers and UnityGain normalization agree", "[dsp][filter][svf][simd]",
                   float, double, xsimd::batch<float>, xsimd::batch<double>) {
    using S = TestType;
    using Scalar = applause::scalar_t<S>;
    const auto cutoff = applause::set1<S>(Scalar(default_cutoff));
    const auto resonant_q = applause::set1<S>(Scalar(default_q));
    const auto target_peak = applause::set1<S>(Scalar(1500.0));
    const double tolerance = std::is_same_v<Scalar, float> ? 2e-5 : 2e-12;
    const double frequency_tolerance = std::is_same_v<Scalar, float> ? 2e-3 : 2e-10;

    SECTION("peak gain and frequency helpers") {
        const double k = 1.0 / default_q;
        const double expected_peak_gain = 2.0 / (k * k * std::sqrt(4.0 / (k * k) - 1.0));
        auto lp = configureFilter(applause::SVFLowpass<S>{}, sample_rate, cutoff, resonant_q);
        auto hp = configureFilter(applause::SVFHighpass<S>{}, sample_rate, cutoff, resonant_q);
        auto bp = configureFilter(applause::SVFBandpass<S>{}, sample_rate, cutoff, resonant_q);
        requireClose(lp.getPeakGain(), applause::set1<S>(Scalar(expected_peak_gain)), tolerance);
        requireClose(hp.getPeakGain(), applause::set1<S>(Scalar(expected_peak_gain)), tolerance);
        requireClose(bp.getPeakGain(), resonant_q, tolerance);

        lp.setPeakFrequency(target_peak);
        hp.setPeakFrequency(target_peak);
        bp.setPeakFrequency(target_peak);
        requireClose(lp.getPeakFrequency(), target_peak, frequency_tolerance);
        requireClose(hp.getPeakFrequency(), target_peak, frequency_tolerance);
        requireClose(bp.getPeakFrequency(), target_peak, frequency_tolerance);

        constexpr std::array peak_probe_frequencies{1250.0, 1500.0, 1750.0};
        const auto lp_response = measuredResponses<S>(lp, peak_probe_frequencies, sample_rate);
        const auto hp_response = measuredResponses<S>(hp, peak_probe_frequencies, sample_rate);
        const auto bp_response = measuredResponses<S>(bp, peak_probe_frequencies, sample_rate);
        for (size_t lane = 0; lane < applause::sample_width_v<S>; ++lane) {
            CAPTURE(lane);
            REQUIRE(std::abs(lp_response[1][lane]) > std::abs(lp_response[0][lane]));
            REQUIRE(std::abs(lp_response[1][lane]) > std::abs(lp_response[2][lane]));
            REQUIRE(std::abs(hp_response[1][lane]) > std::abs(hp_response[0][lane]));
            REQUIRE(std::abs(hp_response[1][lane]) > std::abs(hp_response[2][lane]));
            REQUIRE(std::abs(bp_response[1][lane]) > std::abs(bp_response[0][lane]));
            REQUIRE(std::abs(bp_response[1][lane]) > std::abs(bp_response[2][lane]));
        }

        const auto low_q = applause::set1<S>(Scalar(0.5));
        auto non_resonant_lp = configureFilter(applause::SVFLowpass<S>{}, sample_rate, cutoff, low_q);
        requireClose(non_resonant_lp.getPeakGain(), applause::set1<S>(Scalar(1.0)), tolerance);
        requireClose(non_resonant_lp.getPeakFrequency(), cutoff, tolerance);
    }

    SECTION("UnityGain applies the reciprocal peak gain") {
        const auto signal = testSignal<Scalar>();
        auto requireUnityGain = [&]<applause::StateVariableFilterType Type>() {
            using PlainFilter = applause::StateVariableFilter<S, Type, false>;
            using UnityFilter = applause::StateVariableFilter<S, Type, true>;
            auto plain = configureFilter(PlainFilter{}, sample_rate, cutoff, resonant_q);
            auto unity = configureFilter(UnityFilter{}, sample_rate, cutoff, resonant_q);
            const S peak_gain = plain.getPeakGain();
            for (Scalar value : signal) {
                const auto input = applause::set1<S>(value);
                requireClose(unity.processSample(input), plain.processSample(input) / peak_gain, tolerance);
            }
        };

        requireUnityGain.template operator()<applause::StateVariableFilterType::Lowpass>();
        requireUnityGain.template operator()<applause::StateVariableFilterType::Bandpass>();
        requireUnityGain.template operator()<applause::StateVariableFilterType::Highpass>();
    }

    if constexpr (applause::SimdBatch<S>) {
        SECTION("mixed SIMD lanes cross the resonance threshold safely") {
            alignas(64) std::array<Scalar, S::size> lane_cutoffs{};
            alignas(64) std::array<Scalar, S::size> lane_qs{};
            for (size_t lane = 0; lane < S::size; ++lane) {
                lane_cutoffs[lane] = Scalar(600 + lane * 200);
                lane_qs[lane] = lane % 2 == 0 ? Scalar(0.25) : Scalar(2.0);
            }

            auto simd_lp = configureFilter(applause::SVFLowpass<S>{}, sample_rate, S::load_aligned(lane_cutoffs.data()),
                                           S::load_aligned(lane_qs.data()));
            auto simd_hp = configureFilter(applause::SVFHighpass<S>{}, sample_rate,
                                           S::load_aligned(lane_cutoffs.data()), S::load_aligned(lane_qs.data()));
            const S lp_gain = simd_lp.getPeakGain();
            const S lp_frequency = simd_lp.getPeakFrequency();
            const S hp_frequency = simd_hp.getPeakFrequency();

            for (size_t lane = 0; lane < S::size; ++lane) {
                auto scalar_lp =
                    configureFilter(applause::SVFLowpass<Scalar>{}, sample_rate, lane_cutoffs[lane], lane_qs[lane]);
                auto scalar_hp =
                    configureFilter(applause::SVFHighpass<Scalar>{}, sample_rate, lane_cutoffs[lane], lane_qs[lane]);
                CAPTURE(lane);
                REQUIRE(std::isfinite(lp_gain.get(lane)));
                REQUIRE(std::isfinite(lp_frequency.get(lane)));
                REQUIRE(std::isfinite(hp_frequency.get(lane)));
                REQUIRE(lp_gain.get(lane) == Approx(scalar_lp.getPeakGain()).epsilon(0.0).margin(tolerance));
                // xsimd's atan can differ from libm's by an ulp, which at these
                // frequencies exceeds `tolerance`; frequencies get their own margin.
                REQUIRE(lp_frequency.get(lane) ==
                        Approx(scalar_lp.getPeakFrequency()).epsilon(0.0).margin(frequency_tolerance));
                REQUIRE(hp_frequency.get(lane) ==
                        Approx(scalar_hp.getPeakFrequency()).epsilon(0.0).margin(frequency_tolerance));
            }
        }
    }
}
