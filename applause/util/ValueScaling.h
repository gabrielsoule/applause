#pragma once

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

    [[nodiscard]] float fromNormalized(float norm, float min, float max) const noexcept {
        switch (type) {
        case ValueScale::Frequency:
            return a * std::exp2(norm * b / 12.0f);
        case ValueScale::Time:
            return a * std::pow(10.0f, norm * b);
        case ValueScale::Quadratic:
            return min + (norm * norm) * (max - min);
        case ValueScale::Linear:
        default:
            return min + norm * (max - min);
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
