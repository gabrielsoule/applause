#pragma once

#include <applause/core/ModMatrix.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <xsimd/xsimd.hpp>

class SimdOscillator final {
public:
    using Batch = xsimd::batch<float>;
    using Mask = Batch::batch_bool_type;
    using ModMatrix = applause::ModMatrix<Batch>;

    enum class Waveform : std::uint8_t { Sine, Triangle, Saw, Square };

    static constexpr std::size_t lane_count = 4;
    static_assert(Batch::size == lane_count, "we require four-lane float SIMD");

    void activate(double sample_rate) noexcept {
        assert(std::isfinite(sample_rate) && sample_rate > 0.0);
        if (std::isfinite(sample_rate) && sample_rate > 0.0)
            inverse_sample_rate_ = static_cast<float>(1.0 / sample_rate);
    }

    void bindParameters(ModMatrix& matrix, std::uint16_t matrix_voice, const std::string& prefix) {
        assert(!parameters_bound_);

        const auto* waveform = matrix.findDestination(prefix + "_waveform");
        assert(waveform != nullptr);
        assert(waveform->mode == applause::ModDstMode::Mono);
        waveform_param_ = matrix.getModHandle(waveform->index);

        const auto polyHandle = [&](const std::string& suffix) {
            const auto* destination = matrix.findDestination(prefix + suffix);
            assert(destination != nullptr);
            assert(destination->mode == applause::ModDstMode::Poly);
            return matrix.getModHandle(destination->index, matrix_voice);
        };

        octave_ = polyHandle("_octave");
        semitone_ = polyHandle("_semitone");
        fine_ = polyHandle("_fine");
        level_ = polyHandle("_level");
        pan_ = polyHandle("_pan");
        parameters_bound_ = true;
    }

    void updateFromParameters(Mask active_lanes, Batch base_frequency) noexcept {
        if (parameters_bound_) {
            auto waveform = waveform_param_.getValue();
            if (!std::isfinite(waveform)) waveform = 0.0f;
            waveform = std::trunc(std::clamp(waveform, 0.0f, 3.0f));
            waveform_ = static_cast<Waveform>(static_cast<std::uint8_t>(waveform));

            const Batch zero{0.0f};
            const Batch one{1.0f};
            auto semitones = octave_.getValue() * Batch{12.0f} + semitone_.getValue() + fine_.getValue() * Batch{0.01f};
            semitones = xsimd::select(xsimd::isfinite(semitones), semitones, zero);
            pitch_ratio_ = xsimd::exp2(semitones * Batch{1.0f / 12.0f});

            auto level = level_.getValue();
            level = xsimd::clip(xsimd::select(xsimd::isfinite(level), level, zero), zero, one);

            auto pan = pan_.getValue();
            pan = xsimd::clip(xsimd::select(xsimd::isfinite(pan), pan, zero), Batch{-1.0f}, one);
            const auto angle = (pan + one) * Batch{pi_over_four};
            left_gain_ = level * xsimd::cos(angle);
            right_gain_ = level * xsimd::sin(angle);
        }

        setBaseFrequency(active_lanes, base_frequency);
        if (waveform_ == Waveform::Sine) anchorSine(active_lanes);
    }

    void start(Mask lanes, Batch base_frequency) noexcept {
        reset(lanes);
        setBaseFrequency(lanes, base_frequency);
    }

    void reset(Mask lanes) noexcept {
        const Batch zero{0.0f};
        phase_ = xsimd::select(lanes, zero, phase_);
        sine_ = xsimd::select(lanes, zero, sine_);
        cosine_ = xsimd::select(lanes, Batch{1.0f}, cosine_);
    }

    void kill(Mask lanes) noexcept {
        reset(lanes);
        const Batch zero{0.0f};
        phase_increment_ = xsimd::select(lanes, zero, phase_increment_);
        inverse_phase_increment_ = xsimd::select(lanes, zero, inverse_phase_increment_);
        triangle_width_ = xsimd::select(lanes, zero, triangle_width_);
        inverse_triangle_width_ = xsimd::select(lanes, zero, inverse_triangle_width_);
        triangle_scale_ = xsimd::select(lanes, zero, triangle_scale_);
        sine_step_ = xsimd::select(lanes, zero, sine_step_);
        cosine_step_ = xsimd::select(lanes, Batch{1.0f}, cosine_step_);
    }

    void setFrequency(Mask lanes, Batch frequency) noexcept {
        setPhaseIncrement(lanes, frequency * Batch{inverse_sample_rate_});
    }

    void setBaseFrequency(Mask lanes, Batch base_frequency) noexcept {
        setFrequency(lanes, base_frequency * pitch_ratio_);
    }

    void setPhaseIncrement(Mask lanes, Batch increment) noexcept {
        const Batch zero{0.0f};
        const Batch one{1.0f};
        increment = xsimd::select(xsimd::isfinite(increment), increment, zero);
        increment = xsimd::clip(increment, zero, Batch{0.5f});
        phase_increment_ = xsimd::select(lanes, increment, phase_increment_);

        const auto positive = increment > zero;
        const auto safe_increment = xsimd::select(positive, increment, one);
        const auto inverse = xsimd::select(positive, one / safe_increment, zero);
        inverse_phase_increment_ = xsimd::select(lanes, inverse, inverse_phase_increment_);
        triangle_width_ = xsimd::select(lanes, increment * Batch{2.0f}, triangle_width_);
        inverse_triangle_width_ = xsimd::select(lanes, inverse * Batch{0.5f}, inverse_triangle_width_);
        triangle_scale_ = xsimd::select(lanes, increment * Batch{4.0f / 3.0f}, triangle_scale_);

        if (waveform_ == Waveform::Sine) {
            const auto angle = increment * Batch{two_pi};
            sine_step_ = xsimd::select(lanes, xsimd::sin(angle), sine_step_);
            cosine_step_ = xsimd::select(lanes, xsimd::cos(angle), cosine_step_);
        }
    }

    template <Waveform waveform>
    [[nodiscard]] Batch processSample() noexcept {
        Batch output;
        if constexpr (waveform == Waveform::Sine) {
            output = sine_;
            const auto old_sine = sine_;
            sine_ = xsimd::fma(old_sine, cosine_step_, cosine_ * sine_step_);
            cosine_ = xsimd::fma(cosine_, cosine_step_, -(old_sine * sine_step_));
        } else if constexpr (waveform == Waveform::Triangle) {
            output = triangleSample();
        } else if constexpr (waveform == Waveform::Saw) {
            output = sawSample(phase_);
        } else {
            auto shifted_phase = phase_ + Batch{0.5f};
            shifted_phase = xsimd::select(shifted_phase >= Batch{1.0f}, shifted_phase - Batch{1.0f}, shifted_phase);
            output = sawSample(shifted_phase) - sawSample(phase_);
        }

        output = xsimd::select(phase_increment_ > Batch{0.0f}, output, Batch{0.0f});
        advancePhase();
        return output;
    }

    template <Waveform waveform>
    void renderAdd(Batch& left, Batch& right) noexcept {
        const auto output = processSample<waveform>();
        left += output * left_gain_;
        right += output * right_gain_;
    }

    template <typename Function>
    decltype(auto) dispatch(Function&& function) const noexcept {
        switch (waveform_) {
        case Waveform::Sine:
            return std::forward<Function>(function).template operator()<Waveform::Sine>();
        case Waveform::Triangle:
            return std::forward<Function>(function).template operator()<Waveform::Triangle>();
        case Waveform::Saw:
            return std::forward<Function>(function).template operator()<Waveform::Saw>();
        case Waveform::Square:
            return std::forward<Function>(function).template operator()<Waveform::Square>();
        }

        assert(false);
        return std::forward<Function>(function).template operator()<Waveform::Sine>();
    }

    [[nodiscard]] Batch phase() const noexcept { return phase_; }
    [[nodiscard]] Batch phaseIncrement() const noexcept { return phase_increment_; }

private:
    // Centered DPW forms independently derived from https://doi.org/10.1109/TASL.2009.2026507.
    [[nodiscard]] Batch sawSample(Batch phase) const noexcept {
        const Batch one{1.0f};
        auto output = xsimd::fma(phase, Batch{2.0f}, Batch{-1.0f});
        const auto leading = phase < phase_increment_;
        const auto trailing = phase > one - phase_increment_;
        if (xsimd::any(leading | trailing)) {
            auto correction = one - phase * inverse_phase_increment_;
            correction *= correction;
            output += xsimd::select(leading, correction, Batch{0.0f});

            correction = one - (one - phase) * inverse_phase_increment_;
            correction *= correction;
            output -= xsimd::select(trailing, correction, Batch{0.0f});
        }
        return output;
    }

    [[nodiscard]] Batch triangleSample() const noexcept {
        const Batch one{1.0f};
        auto position = xsimd::fma(phase_, Batch{2.0f}, Batch{-0.5f});
        position = xsimd::select(position >= one, position - Batch{2.0f}, position);
        const auto magnitude = xsimd::abs(position);
        auto output = xsimd::fma(magnitude, Batch{-2.0f}, one);

        const auto peak = magnitude < triangle_width_;
        const auto trough_distance = one - magnitude;
        const auto trough = trough_distance < triangle_width_;
        if (xsimd::any(peak | trough)) {
            const auto zero = Batch{0.0f};
            auto peak_value = xsimd::max(one - magnitude * inverse_triangle_width_, zero);
            peak_value = peak_value * peak_value * peak_value;
            auto trough_value = xsimd::max(one - trough_distance * inverse_triangle_width_, zero);
            trough_value = trough_value * trough_value * trough_value;
            output +=
                triangle_scale_ * (xsimd::select(trough, trough_value, zero) - xsimd::select(peak, peak_value, zero));
        }
        return output;
    }

    void anchorSine(Mask lanes) noexcept {
        const auto angle = phase_ * Batch{two_pi};
        sine_ = xsimd::select(lanes, xsimd::sin(angle), sine_);
        cosine_ = xsimd::select(lanes, xsimd::cos(angle), cosine_);
    }

    void advancePhase() noexcept {
        phase_ += phase_increment_;
        phase_ = xsimd::select(phase_ >= Batch{1.0f}, phase_ - Batch{1.0f}, phase_);
    }

    static constexpr float two_pi = 6.28318530717958647692f;
    static constexpr float pi_over_four = 0.78539816339744830962f;

    Batch phase_{0.0f};
    Batch phase_increment_{0.0f};
    Batch inverse_phase_increment_{0.0f};
    Batch triangle_width_{0.0f};
    Batch inverse_triangle_width_{0.0f};
    Batch triangle_scale_{0.0f};
    Batch sine_{0.0f};
    Batch cosine_{1.0f};
    Batch sine_step_{0.0f};
    Batch cosine_step_{1.0f};
    Batch pitch_ratio_{1.0f};
    Batch left_gain_{0.70710678118654752440f};
    Batch right_gain_{0.70710678118654752440f};
    float inverse_sample_rate_ = 1.0f / 44100.0f;
    applause::ModParamHandle<float> waveform_param_;
    applause::ModParamHandle<Batch> octave_;
    applause::ModParamHandle<Batch> semitone_;
    applause::ModParamHandle<Batch> fine_;
    applause::ModParamHandle<Batch> level_;
    applause::ModParamHandle<Batch> pan_;
    Waveform waveform_ = Waveform::Sine;
    bool parameters_bound_ = false;
};
