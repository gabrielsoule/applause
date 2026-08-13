#pragma once

#include <applause/util/SampleType.h>
#include <cmath>
#include <cstdint>

namespace applause {
enum class ValueScale : uint8_t {
    Linear,
    Frequency,
    Time,
    Quadratic,
};


struct ValueScaling {
    ValueScale type = ValueScale::Linear;
    float a = 0.0f;
    float b = 0.0f;

    [[nodiscard]] float toNormalized(float plain, float min, float max) const noexcept {
        switch (type) {
        case ValueScale::Frequency:
            return 12.0f * std::log2(plain / a) / b;
        case ValueScale::Time:
            return std::log10(plain / a) / b;
        case ValueScale::Quadratic:
            return std::sqrt((plain - min) / (max - min));
        case ValueScale::Linear:
        default:
            return (plain - min) / (max - min);
        }
    }

    template <Sample Signal>
    [[nodiscard]] Signal fromNormalized(Signal norm, float min, float max) const noexcept {
        const auto value = [](float scalar) { return set1<Signal>(scalar); };
        switch (type) {
        case ValueScale::Frequency:
            if constexpr (SimdBatch<Signal>)
                return value(a) * xsimd::exp2(norm * value(b / 12.0f));
            else
                return a * std::exp2(norm * b / 12.0f);
        case ValueScale::Time:
            if constexpr (SimdBatch<Signal>)
                return value(a) * xsimd::pow(value(10.0f), norm * value(b));
            else
                return a * std::pow(10.0f, norm * b);
        case ValueScale::Quadratic:
            return value(min) + (norm * norm) * value(max - min);
        case ValueScale::Linear:
        default:
            return value(min) + norm * value(max - min);
        }
    }

    static ValueScaling linear() { return {ValueScale::Linear, 0.0f, 0.0f}; }

    static ValueScaling frequency(float minHz, float maxHz) {
        float semitones = 12.0f * std::log2(maxHz / minHz);
        return {ValueScale::Frequency, minHz, semitones};
    }

    static ValueScaling time(float minSec, float maxSec) {
        float decades = std::log10(maxSec / minSec);
        return {ValueScale::Time, minSec, decades};
    }

    static ValueScaling quadratic() { return {ValueScale::Quadratic, 0.0f, 0.0f}; }
};

struct ValueScaleInfo {
    float min;
    float max;
    ValueScaling scaling;
};
} // namespace applause
