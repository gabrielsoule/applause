#include "SimdOscillator.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

namespace {
using Waveform = SimdOscillator::Waveform;

constexpr double sample_rate = 48000.0;
constexpr double two_pi = 2.0 * std::numbers::pi_v<double>;

SimdOscillator::Mask laneMask(std::uint64_t bits) { return SimdOscillator::Mask::from_mask(bits); }

double wrapPhase(double phase) { return phase - std::floor(phase); }

double referenceSaw(double phase, double increment) {
    phase = wrapPhase(phase);
    auto output = 2.0 * phase - 1.0;
    if (phase < increment) {
        const auto distance = 1.0 - phase / increment;
        output += distance * distance;
    }
    if (phase > 1.0 - increment) {
        const auto distance = 1.0 - (1.0 - phase) / increment;
        output -= distance * distance;
    }
    return output;
}

double referenceTriangle(double phase, double increment) {
    auto position = 2.0 * wrapPhase(phase) - 0.5;
    if (position >= 1.0) position -= 2.0;

    const auto magnitude = std::abs(position);
    auto output = 1.0 - 2.0 * magnitude;
    const auto width = 2.0 * increment;
    const auto scale = 4.0 * increment / 3.0;
    if (magnitude < width) {
        const auto peak = std::max(1.0 - magnitude / width, 0.0);
        output -= scale * peak * peak * peak;
    }

    const auto trough_distance = 1.0 - magnitude;
    if (trough_distance < width) {
        const auto trough = std::max(1.0 - trough_distance / width, 0.0);
        output += scale * trough * trough * trough;
    }
    return output;
}

template <Waveform waveform>
double referenceSample(double phase, double increment) {
    if constexpr (waveform == Waveform::Sine) {
        return std::sin(two_pi * phase);
    } else if (increment == 0.0) {
        return 0.0;
    } else if constexpr (waveform == Waveform::Triangle) {
        return referenceTriangle(phase, increment);
    } else if constexpr (waveform == Waveform::Saw) {
        return referenceSaw(phase, increment);
    } else {
        static_assert(waveform == Waveform::Square);
        return referenceSaw(phase + 0.5, increment) - referenceSaw(phase, increment);
    }
}

template <Waveform waveform>
void requireMatchesScalarReference() {
    SimdOscillator oscillator;
    oscillator.activate(sample_rate);

    const std::array<float, SimdOscillator::lane_count> frequencies{0.0f, 20.0f, 440.0f, 23952.0f};
    const auto frequency_batch = SimdOscillator::Batch::load_unaligned(frequencies.data());
    oscillator.start(laneMask(0b1111), frequency_batch);

    for (std::size_t sample = 0; sample < 4096; ++sample) {
        if (sample != 0 && sample % 64 == 0) oscillator.updateFromParameters(laneMask(0b1111), frequency_batch);
        const auto phase = oscillator.phase();
        const auto increment = oscillator.phaseIncrement();
        const auto output = oscillator.processSample<waveform>();

        for (std::size_t lane = 0; lane < SimdOscillator::lane_count; ++lane) {
            const auto expected = referenceSample<waveform>(phase.get(lane), increment.get(lane));
            CAPTURE(waveform, sample, lane, phase.get(lane), increment.get(lane), output.get(lane), expected);
            REQUIRE(std::isfinite(output.get(lane)));
            REQUIRE(std::abs(output.get(lane)) <= 1.001f);
            REQUIRE(output.get(lane) == Catch::Approx(expected).margin(8.0e-4));
        }
    }
}

template <Waveform waveform>
void requireMaskedLifecycle() {
    SimdOscillator oscillator;
    oscillator.activate(sample_rate);

    const std::array<float, SimdOscillator::lane_count> frequencies{4800.0f, 7200.0f, 9600.0f, 12000.0f};
    const auto frequency_batch = SimdOscillator::Batch::load_unaligned(frequencies.data());
    oscillator.start(laneMask(0b0101), frequency_batch);

    for (int sample = 0; sample < 7; ++sample) {
        const auto output = oscillator.processSample<waveform>();
        for (std::size_t lane = 0; lane < SimdOscillator::lane_count; ++lane) REQUIRE(std::isfinite(output.get(lane)));
    }

    const auto sibling_phase = oscillator.phase().get(2);
    oscillator.kill(laneMask(0b0001));
    oscillator.start(laneMask(0b0010), frequency_batch);

    const auto output = oscillator.processSample<waveform>();
    REQUIRE(output.get(0) == 0.0f);
    REQUIRE(output.get(1) == Catch::Approx(referenceSample<waveform>(0.0, 7200.0 / sample_rate)).margin(8.0e-4));
    REQUIRE(oscillator.phase().get(0) == 0.0f);
    REQUIRE(oscillator.phaseIncrement().get(0) == 0.0f);
    REQUIRE(oscillator.phase().get(2) == Catch::Approx(wrapPhase(sibling_phase + 9600.0 / sample_rate)).margin(1.0e-6));

    oscillator.reset(laneMask(0b0100));
    REQUIRE(oscillator.phase().get(2) == 0.0f);
    REQUIRE(oscillator.phaseIncrement().get(2) == Catch::Approx(9600.0 / sample_rate));
}

void fft(std::vector<std::complex<double>>& values) {
    const auto size = values.size();
    for (std::size_t i = 1, reversed = 0; i < size; ++i) {
        auto bit = size >> 1;
        while (reversed & bit) {
            reversed ^= bit;
            bit >>= 1;
        }
        reversed ^= bit;
        if (i < reversed) std::swap(values[i], values[reversed]);
    }

    for (std::size_t length = 2; length <= size; length <<= 1) {
        const auto angle = -two_pi / static_cast<double>(length);
        const std::complex<double> root{std::cos(angle), std::sin(angle)};
        for (std::size_t offset = 0; offset < size; offset += length) {
            std::complex<double> rotation{1.0, 0.0};
            for (std::size_t i = 0; i < length / 2; ++i) {
                const auto even = values[offset + i];
                const auto odd = values[offset + i + length / 2] * rotation;
                values[offset + i] = even + odd;
                values[offset + i + length / 2] = even - odd;
                rotation *= root;
            }
        }
    }
}

double aliasEnergyDb(const std::vector<float>& samples, std::size_t fundamental_bin, bool odd_harmonics_only) {
    std::vector<std::complex<double>> spectrum;
    spectrum.reserve(samples.size());
    for (const auto sample : samples) spectrum.emplace_back(sample, 0.0);
    fft(spectrum);

    std::vector<bool> valid_bin(samples.size(), false);
    valid_bin[0] = true;
    const auto highest_harmonic = (samples.size() / 2 - 1) / fundamental_bin;
    for (std::size_t harmonic = 1; harmonic <= highest_harmonic; ++harmonic) {
        if (odd_harmonics_only && harmonic % 2 == 0) continue;
        const auto bin = harmonic * fundamental_bin;
        valid_bin[bin] = true;
        valid_bin[samples.size() - bin] = true;
    }

    double valid_energy = 0.0;
    double alias_energy = 0.0;
    for (std::size_t bin = 0; bin < spectrum.size(); ++bin) {
        const auto energy = std::norm(spectrum[bin]);
        (valid_bin[bin] ? valid_energy : alias_energy) += energy;
    }
    return 10.0 * std::log10(alias_energy / valid_energy);
}

template <Waveform waveform>
std::vector<float> renderWaveform(std::size_t sample_count, std::size_t fundamental_bin) {
    SimdOscillator oscillator;
    oscillator.activate(static_cast<double>(sample_count));
    oscillator.start(laneMask(0b0001), SimdOscillator::Batch{static_cast<float>(fundamental_bin)});

    std::vector<float> samples(sample_count);
    for (auto& sample : samples) sample = oscillator.processSample<waveform>().get(0);
    return samples;
}

template <Waveform waveform>
std::vector<float> renderNaiveWaveform(std::size_t sample_count, std::size_t fundamental_bin) {
    std::vector<float> samples(sample_count);
    auto phase = 0.0;
    const auto increment = static_cast<double>(fundamental_bin) / static_cast<double>(sample_count);

    for (auto& sample : samples) {
        const auto bipolar_phase = 2.0 * phase - 1.0;
        if constexpr (waveform == Waveform::Saw) {
            sample = static_cast<float>(bipolar_phase);
        } else if constexpr (waveform == Waveform::Square) {
            sample = bipolar_phase < 0.0 ? 1.0f : -1.0f;
        } else {
            static_assert(waveform == Waveform::Triangle);
            sample = static_cast<float>(1.0 - 4.0 * std::abs(phase - 0.5));
        }
        phase = wrapPhase(phase + increment);
    }
    return samples;
}

template <Waveform waveform>
void requireAliasReduction(bool odd_harmonics_only) {
    constexpr std::size_t sample_count = 8192;
    constexpr std::array<std::size_t, 3> fundamental_bins{173, 683, 1501};

    for (const auto fundamental_bin : fundamental_bins) {
        const auto dpw = renderWaveform<waveform>(sample_count, fundamental_bin);
        const auto naive = renderNaiveWaveform<waveform>(sample_count, fundamental_bin);
        const auto dpw_alias = aliasEnergyDb(dpw, fundamental_bin, odd_harmonics_only);
        const auto naive_alias = aliasEnergyDb(naive, fundamental_bin, odd_harmonics_only);

        CAPTURE(waveform, fundamental_bin, dpw_alias, naive_alias);
        REQUIRE(std::ranges::all_of(dpw, [](float sample) { return std::isfinite(sample); }));
        REQUIRE(dpw_alias <= naive_alias - 6.0);
    }
}
}  // namespace

TEST_CASE("SimdOscillator waveforms match independent scalar formulas", "[dsp][simd-poly-synth][oscillator]") {
    requireMatchesScalarReference<Waveform::Sine>();
    requireMatchesScalarReference<Waveform::Triangle>();
    requireMatchesScalarReference<Waveform::Saw>();
    requireMatchesScalarReference<Waveform::Square>();
}

TEST_CASE("SimdOscillator waveform lifecycle masks preserve sibling lanes", "[dsp][simd-poly-synth][oscillator]") {
    requireMaskedLifecycle<Waveform::Sine>();
    requireMaskedLifecycle<Waveform::Triangle>();
    requireMaskedLifecycle<Waveform::Saw>();
    requireMaskedLifecycle<Waveform::Square>();
}

TEST_CASE("SimdOscillator DPW waveforms reduce alias energy", "[dsp][simd-poly-synth][oscillator][spectral]") {
    requireAliasReduction<Waveform::Triangle>(true);
    requireAliasReduction<Waveform::Saw>(false);
    requireAliasReduction<Waveform::Square>(true);
}
