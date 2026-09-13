#pragma once

#include "SimdAdsr.h"
#include "SimdOscillator.h"

#include <applause/core/ModMatrix.h>
#include <applause/dsp/BufferView.h>
#include <applause/dsp/Note.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <xsimd/xsimd.hpp>

class BatchedVoice final {
public:
    using Batch = SimdAdsr::Batch;
    using Mask = SimdAdsr::Mask;
    using ModMatrix = applause::ModMatrix<Batch>;

    static constexpr std::size_t lane_count = SimdAdsr::lane_count;

    void activate(double sample_rate) noexcept {
        assert(sample_rate > 0.0);
        envelope_.activate(sample_rate);
        for (auto& oscillator : oscillators_) oscillator.activate(sample_rate);
    }

    void bindParameters(ModMatrix& matrix, std::uint16_t matrix_voice) {
        envelope_.bindParameters(matrix, matrix_voice);
        oscillators_[0].bindParameters(matrix, matrix_voice, "osc1");
        oscillators_[1].bindParameters(matrix, matrix_voice, "osc2");
    }

    void preProcess(Mask lanes, const applause::Note& note) noexcept {
        timbre_ = xsimd::select(lanes, Batch{static_cast<float>(note.brightness)}, timbre_);
        pressure_ = xsimd::select(lanes, Batch{static_cast<float>(note.pressure)}, pressure_);
    }

    void start(Mask lanes, const applause::Note& note) noexcept {
        assert(xsimd::none(occupiedMask() & lanes));
        assert(lanes.mask() != 0);

        auto frequency = static_cast<float>(note.getFrequency());
        if (!std::isfinite(frequency) || frequency < 0.0f) frequency = 0.0f;
        const auto velocity = static_cast<float>(std::clamp(note.note_on_velocity, 0.0, 1.0));
        const auto volume = static_cast<float>(std::clamp(note.volume, 0.0, 4.0));
        base_frequency_ = xsimd::select(lanes, Batch{frequency}, base_frequency_);
        velocity_ = xsimd::select(lanes, Batch{velocity}, velocity_);
        note_volume_ = xsimd::select(lanes, Batch{volume}, note_volume_);
        setNotePan(lanes, note.pan);

        for (auto& oscillator : oscillators_) oscillator.start(lanes, Batch{frequency});
        envelope_.start(lanes);
    }

    void kill(Mask lanes) noexcept {
        resetLanes(lanes);
        envelope_.kill(lanes);
    }

    [[nodiscard]] Mask release(Mask lanes) noexcept {
        const auto finished = envelope_.release(lanes);
        resetLanes(finished);
        return finished;
    }

    void expressionChanged(Mask lanes, applause::Note::Expression expression, const applause::Note& note) noexcept {
        switch (expression) {
        case applause::Note::Expression::Tuning:
            {
                auto frequency = static_cast<float>(note.getFrequency());
                if (!std::isfinite(frequency) || frequency < 0.0f) frequency = 0.0f;
                base_frequency_ = xsimd::select(lanes, Batch{frequency}, base_frequency_);
                for (auto& oscillator : oscillators_) oscillator.setBaseFrequency(lanes, Batch{frequency});
                break;
            }
        case applause::Note::Expression::Volume:
            note_volume_ =
                xsimd::select(lanes, Batch{static_cast<float>(std::clamp(note.volume, 0.0, 4.0))}, note_volume_);
            break;
        case applause::Note::Expression::Pan:
            setNotePan(lanes, note.pan);
            break;
        default:
            break;
        }
    }

    [[nodiscard]] bool isOccupied(Mask lanes) const noexcept { return xsimd::any(occupiedMask() & lanes); }
    [[nodiscard]] Mask occupiedMask() const noexcept { return envelope_.activeMask(); }
    [[nodiscard]] bool empty() const noexcept { return xsimd::none(occupiedMask()); }
    [[nodiscard]] Batch envelopeValue() const noexcept { return envelope_.value(); }
    [[nodiscard]] Batch timbreValue() const noexcept { return timbre_; }
    [[nodiscard]] Batch pressureValue() const noexcept { return pressure_; }

    [[nodiscard]] Mask renderAdd(applause::BufferView<float> output, int start_sample, int num_samples) noexcept {
        assert(output.numChannels() >= 2);
        const auto active = occupiedMask();
        for (auto& oscillator : oscillators_) oscillator.updateFromParameters(active, base_frequency_);

        // template shenanigans to avoid a conditional switch(waveform_parameter) in the hot DSP path
        return oscillators_[0].dispatch([&]<SimdOscillator::Waveform first>() noexcept {
            return oscillators_[1].dispatch([&]<SimdOscillator::Waveform second>() noexcept {
                return renderRange<first, second>(output, start_sample, num_samples);
            });
        });
    }

private:
    // template shenanigans to avoid a conditional switch(waveform_parameter) in the hot DSP path
    template <SimdOscillator::Waveform first, SimdOscillator::Waveform second>
    [[nodiscard]] Mask renderRange(applause::BufferView<float> output, int start_sample, int num_samples) noexcept {
        auto active = occupiedMask();
        const Batch zero{0.0f};
        auto left = output.channel(0);
        auto right = output.channel(1);
        Mask finished{false};

        for (int rendered = 0; rendered < num_samples; ++rendered) {
            const auto envelope = envelope_.processSample();
            const auto stopped = envelope.finished;
            if (xsimd::any(stopped)) {
                finished |= stopped;
                active &= ~stopped;
                resetLanes(stopped);
                if (xsimd::none(active)) break;
            }

            Batch left_voices{0.0f};
            Batch right_voices{0.0f};
            oscillators_[0].template renderAdd<first>(left_voices, right_voices);
            oscillators_[1].template renderAdd<second>(left_voices, right_voices);

            const auto amplitude = envelope.value * velocity_ * note_volume_;
            left_voices = xsimd::select(active, left_voices * amplitude * note_pan_left_, zero);
            right_voices = xsimd::select(active, right_voices * amplitude * note_pan_right_, zero);
            const auto frame = static_cast<std::size_t>(start_sample + rendered);
            left.add(frame, xsimd::reduce_add(left_voices));
            right.add(frame, xsimd::reduce_add(right_voices));
        }

        return finished;
    }

    void setNotePan(Mask lanes, double pan) noexcept {
        const auto angle = static_cast<float>(std::clamp(pan, 0.0, 1.0)) * half_pi;
        note_pan_left_ = xsimd::select(lanes, Batch{std::cos(angle) * sqrt_two}, note_pan_left_);
        note_pan_right_ = xsimd::select(lanes, Batch{std::sin(angle) * sqrt_two}, note_pan_right_);
    }

    void resetLanes(Mask lanes) noexcept {
        const Batch zero{0.0f};
        for (auto& oscillator : oscillators_) oscillator.kill(lanes);
        base_frequency_ = xsimd::select(lanes, zero, base_frequency_);
        velocity_ = xsimd::select(lanes, zero, velocity_);
        note_volume_ = xsimd::select(lanes, zero, note_volume_);
        note_pan_left_ = xsimd::select(lanes, zero, note_pan_left_);
        note_pan_right_ = xsimd::select(lanes, zero, note_pan_right_);
        timbre_ = xsimd::select(lanes, zero, timbre_);
        pressure_ = xsimd::select(lanes, zero, pressure_);
    }

    static constexpr float half_pi = 1.57079632679489661923f;
    static constexpr float sqrt_two = 1.41421356237309504880f;

    std::array<SimdOscillator, 2> oscillators_{};
    Batch base_frequency_{0.0f};
    Batch velocity_{0.0f};
    Batch note_volume_{0.0f};
    Batch note_pan_left_{0.0f};
    Batch note_pan_right_{0.0f};
    Batch timbre_{0.0f};
    Batch pressure_{0.0f};
    SimdAdsr envelope_;
};
