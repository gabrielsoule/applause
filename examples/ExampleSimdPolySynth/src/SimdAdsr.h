#pragma once

#include <applause/core/ModMatrix.h>

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include <xsimd/xsimd.hpp>

struct SimdAdsrSettings {
    std::uint32_t attack_samples = 0;
    std::uint32_t decay_samples = 0;
    std::uint32_t release_samples = 0;
    float sustain = 1.0f;
    float curve = -4.0f;
};

class SimdAdsr final {
public:
    using Batch = xsimd::batch<float>;
    using Mask = Batch::batch_bool_type;
    using ModMatrix = applause::ModMatrix<Batch>;

    struct Result {
        Batch value;
        Mask finished;
    };

    static constexpr std::size_t lane_count = 4;
    static_assert(Batch::size == lane_count, "SimdAdsr requires four-lane float SIMD");

    void activate(double sample_rate) noexcept {
        assert(std::isfinite(sample_rate) && sample_rate > 0.0);
        if (std::isfinite(sample_rate) && sample_rate > 0.0) sample_rate_ = sample_rate;
    }

    void bindParameters(ModMatrix& matrix, std::uint16_t matrix_voice) {
        assert(!parameters_bound_);

        const auto handle = [&](const char* id) {
            const auto* destination = matrix.findDestination(id);
            assert(destination != nullptr);
            assert(destination->mode == applause::ModDstMode::Poly);
            return matrix.getModHandle(destination->index, matrix_voice);
        };

        attack_param_ = handle("attack");
        decay_param_ = handle("decay");
        sustain_param_ = handle("sustain");
        release_param_ = handle("release");
        parameters_bound_ = true;
    }

    void start(Mask lanes) noexcept {
        auto mask = lanes.mask();
        while (mask != 0) {
            const auto lane = static_cast<std::size_t>(std::countr_zero(mask));
            const auto lane_mask = Mask::from_mask(std::uint64_t{1} << lane);
            start(lane_mask, parameterSettings(lane));
            mask &= mask - 1;
        }
    }

    void start(Mask lanes, const SimdAdsrSettings& parameters) noexcept {
        assert(xsimd::none(activeMask() & lanes));
        assert(parameters.sustain >= 0.0f && parameters.sustain <= 1.0f);

        const float sustain = std::clamp(parameters.sustain, 0.0f, 1.0f);
        const auto decay = parameters.decay_samples == 0
            ? std::pair{Batch{1.0f}, Batch{0.0f}}
            : segment(Batch{1.0f}, Batch{sustain}, parameters.decay_samples, parameters.curve);

        sustain_ = xsimd::select(lanes, Batch{sustain}, sustain_);
        decay_multiplier_ = xsimd::select(lanes, decay.first, decay_multiplier_);
        decay_addend_ = xsimd::select(lanes, decay.second, decay_addend_);
        decay_samples_ =
            xsimd::select(lanes, Batch{static_cast<float>(parameters.decay_samples)}, decay_samples_);
        value_ = xsimd::select(lanes, Batch{0.0f}, value_);

        if (parameters.attack_samples == 0) {
            enterDecayOrSustain(lanes);
        } else {
            const auto [multiplier, addend] =
                segment(Batch{0.0f}, Batch{1.0f}, parameters.attack_samples, parameters.curve);
            setMoving(lanes, multiplier, addend, Batch{static_cast<float>(parameters.attack_samples)}, attack_stage);
        }
        updateMoving();
    }

    [[nodiscard]] Mask release(Mask lanes) noexcept {
        auto mask = lanes.mask();
        Mask finished{false};
        while (mask != 0) {
            const auto lane = static_cast<std::size_t>(std::countr_zero(mask));
            const auto lane_mask = Mask::from_mask(std::uint64_t{1} << lane);
            const auto settings = parameterSettings(lane);
            finished |= release(lane_mask, settings.release_samples, settings.curve);
            mask &= mask - 1;
        }
        return finished;
    }

    [[nodiscard]] Mask release(Mask lanes, std::uint32_t samples, float curve) noexcept {
        assert(xsimd::none(lanes & ~activeMask()));
        if (samples == 0) {
            kill(lanes);
            return lanes;
        }

        const Mask finished = lanes & (value_ <= Batch{0.0f});
        const Mask releasing = lanes & ~finished;
        reset(finished);

        const auto [multiplier, addend] = segment(value_, Batch{0.0f}, samples, curve);
        setMoving(releasing, multiplier, addend, Batch{static_cast<float>(samples)}, release_stage);
        updateMoving();
        return finished;
    }

    void kill(Mask lanes) noexcept {
        reset(lanes);
        updateMoving();
    }

    [[nodiscard]] Result processSample() noexcept {
        if (!has_moving_) return {value_, Mask{false}};

        const Batch zero{0.0f};
        const Mask moving = remaining_ > zero;
        value_ = xsimd::fma(multiplier_, value_, addend_);
        remaining_ = xsimd::select(moving, remaining_ - Batch{1.0f}, remaining_);

        const Mask done = moving & (remaining_ <= zero);
        if (xsimd::none(done)) return {value_, Mask{false}};

        const Mask attack_done = done & attackMask();
        const Mask decay_done = done & decayMask();
        const Mask release_done = done & releaseMask();
        enterDecayOrSustain(attack_done);
        enterSustain(decay_done);
        reset(release_done);
        updateMoving();
        return {value_, release_done};
    }

    [[nodiscard]] Batch value() const noexcept { return value_; }
    [[nodiscard]] Mask activeMask() const noexcept { return stage_ != Batch{idle_stage}; }
    [[nodiscard]] Mask attackMask() const noexcept { return stage_ == Batch{attack_stage}; }
    [[nodiscard]] Mask decayMask() const noexcept { return stage_ == Batch{decay_stage}; }
    [[nodiscard]] Mask sustainMask() const noexcept { return stage_ == Batch{sustain_stage}; }
    [[nodiscard]] Mask releaseMask() const noexcept { return stage_ == Batch{release_stage}; }

private:
    [[nodiscard]] SimdAdsrSettings parameterSettings(std::size_t lane) const noexcept {
        if (!parameters_bound_) return {};

        const auto samples = [this](float seconds) {
            const auto rounded = std::max(0.0, std::round(static_cast<double>(seconds) * sample_rate_));
            return static_cast<std::uint32_t>(
                std::min(rounded, static_cast<double>(std::numeric_limits<std::uint32_t>::max())));
        };
        return {samples(attack_param_.getValue().get(lane)),
                samples(decay_param_.getValue().get(lane)),
                samples(release_param_.getValue().get(lane)),
                std::clamp(sustain_param_.getValue().get(lane), 0.0f, 1.0f), envelope_curve};
    }

    static constexpr float idle_stage = 0.0f;
    static constexpr float attack_stage = 1.0f;
    static constexpr float decay_stage = 2.0f;
    static constexpr float sustain_stage = 3.0f;
    static constexpr float release_stage = 4.0f;
    static constexpr float envelope_curve = -4.0f;

    [[nodiscard]] static std::pair<Batch, Batch> segment(Batch start, Batch end, std::uint32_t samples,
                                                         float curve) noexcept {
        assert(samples != 0);
        curve = std::clamp(curve, -8.0f, 8.0f);
        if (std::abs(curve) < 0.01f)
            return {Batch{1.0f}, (end - start) / Batch{static_cast<float>(samples)}};

        const float step = std::expm1(curve / static_cast<float>(samples));
        return {Batch{1.0f + step},
                Batch{-step} * start + (end - start) * Batch{step / std::expm1(curve)}};
    }

    void setMoving(Mask lanes, Batch multiplier, Batch addend, Batch samples, float stage) noexcept {
        multiplier_ = xsimd::select(lanes, multiplier, multiplier_);
        addend_ = xsimd::select(lanes, addend, addend_);
        remaining_ = xsimd::select(lanes, samples, remaining_);
        stage_ = xsimd::select(lanes, Batch{stage}, stage_);
    }

    void enterDecayOrSustain(Mask lanes) noexcept {
        value_ = xsimd::select(lanes, Batch{1.0f}, value_);
        const Mask decay = lanes & (decay_samples_ > Batch{0.0f});
        setMoving(decay, decay_multiplier_, decay_addend_, decay_samples_, decay_stage);
        enterSustain(lanes & ~decay);
    }

    void enterSustain(Mask lanes) noexcept {
        value_ = xsimd::select(lanes, sustain_, value_);
        multiplier_ = xsimd::select(lanes, Batch{1.0f}, multiplier_);
        addend_ = xsimd::select(lanes, Batch{0.0f}, addend_);
        remaining_ = xsimd::select(lanes, Batch{0.0f}, remaining_);
        stage_ = xsimd::select(lanes, Batch{sustain_stage}, stage_);
    }

    void reset(Mask lanes) noexcept {
        value_ = xsimd::select(lanes, Batch{0.0f}, value_);
        stage_ = xsimd::select(lanes, Batch{idle_stage}, stage_);
        multiplier_ = xsimd::select(lanes, Batch{1.0f}, multiplier_);
        addend_ = xsimd::select(lanes, Batch{0.0f}, addend_);
        remaining_ = xsimd::select(lanes, Batch{0.0f}, remaining_);
        sustain_ = xsimd::select(lanes, Batch{0.0f}, sustain_);
        decay_multiplier_ = xsimd::select(lanes, Batch{1.0f}, decay_multiplier_);
        decay_addend_ = xsimd::select(lanes, Batch{0.0f}, decay_addend_);
        decay_samples_ = xsimd::select(lanes, Batch{0.0f}, decay_samples_);
    }

    void updateMoving() noexcept { has_moving_ = xsimd::any(remaining_ > Batch{0.0f}); }

    Batch value_{0.0f};
    Batch stage_{idle_stage};
    Batch multiplier_{1.0f};
    Batch addend_{0.0f};
    Batch remaining_{0.0f};
    Batch sustain_{0.0f};
    Batch decay_multiplier_{1.0f};
    Batch decay_addend_{0.0f};
    Batch decay_samples_{0.0f};
    double sample_rate_ = 44100.0;
    applause::ModParamHandle<Batch> attack_param_;
    applause::ModParamHandle<Batch> decay_param_;
    applause::ModParamHandle<Batch> sustain_param_;
    applause::ModParamHandle<Batch> release_param_;
    bool parameters_bound_ = false;
    bool has_moving_ = false;
};
