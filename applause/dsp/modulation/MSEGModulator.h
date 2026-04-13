#pragma once

#include "MSEGCurve.h"
#include <algorithm>
#include <cmath>

namespace applause {

/**
 * The audio-thread part of the MSEG modulation source system.
 *
 *
*/
class MSEGModulator {
public:
    MSEGModulator() = default;

    MSEGModulator(MSEGCurve<>* curve, float sample_rate)
        : curve_(curve), sample_rate_(sample_rate) {}

    float process(int num_samples) {
        advancePhase(num_samples);
        value_ = curve_->evaluate(phase_);
        return value_;
    }

    void reset() {
        phase_ = 0.0f;
        value_ = 0.0f;
    }

    void setRate(float hz) { rate_ = hz; }
    void setSampleRate(float sr) { sample_rate_ = sr; }
    void setCurve(MSEGCurve<>* curve) { curve_ = curve; }

    [[nodiscard]] float phase() const { return phase_; }
    [[nodiscard]] float value() const { return value_; }

private:
    void advancePhase(int num_samples) {
        phase_ += (rate_ / sample_rate_) * static_cast<float>(num_samples);
        if (curve_->loop)
            phase_ = std::fmod(phase_, 1.0f);
        else
            phase_ = std::min(phase_, 1.0f);
    }

    MSEGCurve<>* curve_ = nullptr;
    float phase_ = 0.0f;
    float rate_ = 1.0f;
    float sample_rate_ = 44100.0f;
    float value_ = 0.0f;
};

} // namespace applause
