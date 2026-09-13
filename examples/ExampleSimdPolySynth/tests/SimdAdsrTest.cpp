#include "SimdAdsr.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace {
using MaskValue = std::uint64_t;

constexpr std::size_t lane_count = 4;
constexpr MaskValue lane_mask = 0x0f;
constexpr double tolerance = 2.0e-5;

SimdAdsr::Mask laneMask(std::size_t lane) {
    return SimdAdsr::Mask::from_mask(MaskValue{1} << lane);
}

void startLane(SimdAdsr& envelope, std::size_t lane, const SimdAdsrSettings& parameters) {
    envelope.start(laneMask(lane), parameters);
}

bool releaseLane(SimdAdsr& envelope, std::size_t lane, std::uint32_t samples, float curve) {
    return xsimd::any(envelope.release(laneMask(lane), samples, curve));
}

void killLane(SimdAdsr& envelope, std::size_t lane) {
    envelope.kill(laneMask(lane));
}

enum class Stage : std::uint8_t { idle, attack, decay, sustain, release };

struct OracleLane {
    Stage stage = Stage::idle;
    std::uint32_t elapsed = 0;
    std::uint32_t attack_samples = 0;
    std::uint32_t decay_samples = 0;
    std::uint32_t release_samples = 0;
    double sustain = 0.0;
    double curve = 0.0;
    double release_curve = 0.0;
    double release_start = 0.0;
    double value = 0.0;
};

struct OracleResult {
    std::array<double, lane_count> value{};
    MaskValue finished_mask = 0;
};

double curveProgress(std::uint32_t elapsed, std::uint32_t samples, float curve) {
    curve = std::clamp(curve, -8.0f, 8.0f);
    const double progress = static_cast<double>(elapsed) / samples;
    if (std::abs(curve) < 0.01f) return progress;
    return std::expm1(static_cast<double>(curve) * progress) / std::expm1(static_cast<double>(curve));
}

class AdsrOracle {
public:
    void startLane(std::size_t lane, const SimdAdsrSettings& parameters) {
        auto& state = lanes_[lane];
        state = {};
        state.attack_samples = parameters.attack_samples;
        state.decay_samples = parameters.decay_samples;
        state.sustain = std::clamp(parameters.sustain, 0.0f, 1.0f);
        state.curve = parameters.curve;

        if (state.attack_samples != 0) {
            state.stage = Stage::attack;
            return;
        }
        enterDecayOrSustain(state);
    }

    bool releaseLane(std::size_t lane, std::uint32_t samples, float curve) {
        auto& state = lanes_[lane];
        if (samples == 0 || state.value <= 0.0) {
            state = {};
            return true;
        }

        state.stage = Stage::release;
        state.elapsed = 0;
        state.release_samples = samples;
        state.release_curve = curve;
        state.release_start = state.value;
        return false;
    }

    void killLane(std::size_t lane) { lanes_[lane] = {}; }

    OracleResult processSample() {
        OracleResult result;
        for (std::size_t lane = 0; lane < lane_count; ++lane) {
            auto& state = lanes_[lane];
            switch (state.stage) {
            case Stage::attack:
                if (++state.elapsed == state.attack_samples) {
                    enterDecayOrSustain(state);
                } else {
                    state.value = curveProgress(state.elapsed, state.attack_samples, static_cast<float>(state.curve));
                }
                break;
            case Stage::decay:
                if (++state.elapsed == state.decay_samples) {
                    enterSustain(state);
                } else {
                    const double progress =
                        curveProgress(state.elapsed, state.decay_samples, static_cast<float>(state.curve));
                    state.value = 1.0 + (state.sustain - 1.0) * progress;
                }
                break;
            case Stage::release:
                if (++state.elapsed == state.release_samples) {
                    state = {};
                    result.finished_mask |= MaskValue{1} << lane;
                } else {
                    const double progress =
                        curveProgress(state.elapsed, state.release_samples, static_cast<float>(state.release_curve));
                    state.value = state.release_start * (1.0 - progress);
                }
                break;
            case Stage::idle:
            case Stage::sustain:
                break;
            }
            result.value[lane] = state.value;
        }
        return result;
    }

    [[nodiscard]] std::array<double, lane_count> value() const {
        std::array<double, lane_count> values{};
        for (std::size_t lane = 0; lane < lane_count; ++lane) values[lane] = lanes_[lane].value;
        return values;
    }

    [[nodiscard]] MaskValue activeMask() const {
        return attackMask() | decayMask() | sustainMask() | releaseMask();
    }
    [[nodiscard]] MaskValue attackMask() const { return stageMask(Stage::attack); }
    [[nodiscard]] MaskValue decayMask() const { return stageMask(Stage::decay); }
    [[nodiscard]] MaskValue sustainMask() const { return stageMask(Stage::sustain); }
    [[nodiscard]] MaskValue releaseMask() const { return stageMask(Stage::release); }

private:
    static void enterDecayOrSustain(OracleLane& state) {
        state.value = 1.0;
        state.elapsed = 0;
        if (state.decay_samples != 0) {
            state.stage = Stage::decay;
            return;
        }
        enterSustain(state);
    }

    static void enterSustain(OracleLane& state) {
        state.stage = Stage::sustain;
        state.elapsed = 0;
        state.value = state.sustain;
    }

    [[nodiscard]] MaskValue stageMask(Stage stage) const {
        MaskValue mask = 0;
        for (std::size_t lane = 0; lane < lane_count; ++lane) {
            if (lanes_[lane].stage == stage) mask |= MaskValue{1} << lane;
        }
        return mask;
    }

    std::array<OracleLane, lane_count> lanes_{};
};

std::array<float, lane_count> unpack(SimdAdsr::Batch value) {
    alignas(SimdAdsr::Batch::arch_type::alignment()) std::array<float, lane_count> values{};
    value.store_aligned(values.data());
    return values;
}

void requireBatch(SimdAdsr::Batch actual, const std::array<double, lane_count>& expected, MaskValue active_mask) {
    const auto values = unpack(actual);
    for (std::size_t lane = 0; lane < lane_count; ++lane) {
        CAPTURE(lane);
        if ((active_mask & (MaskValue{1} << lane)) == 0)
            REQUIRE(values[lane] == 0.0f);
        else
            REQUIRE(values[lane] == Catch::Approx(expected[lane]).epsilon(0.0).margin(tolerance));
    }
}

void requireState(const SimdAdsr& actual, const AdsrOracle& expected) {
    REQUIRE(actual.activeMask().mask() == expected.activeMask());
    REQUIRE(actual.attackMask().mask() == expected.attackMask());
    REQUIRE(actual.decayMask().mask() == expected.decayMask());
    REQUIRE(actual.sustainMask().mask() == expected.sustainMask());
    REQUIRE(actual.releaseMask().mask() == expected.releaseMask());
    REQUIRE((actual.activeMask().mask() & ~lane_mask) == 0);
    requireBatch(actual.value(), expected.value(), expected.activeMask());
}

MaskValue processAndRequire(SimdAdsr& actual, AdsrOracle& expected) {
    const auto actual_result = actual.processSample();
    const auto expected_result = expected.processSample();
    REQUIRE(actual_result.finished.mask() == expected_result.finished_mask);
    requireBatch(actual_result.value, expected_result.value, expected.activeMask());
    requireState(actual, expected);
    return actual_result.finished.mask();
}
}  // namespace

TEST_CASE("SimdAdsr handles zero and exact stage lengths", "[dsp][simd-adsr]") {
    STATIC_REQUIRE(SimdAdsr::Batch::size == lane_count);

    SimdAdsr actual;
    AdsrOracle expected;
    const std::array<SimdAdsrSettings, lane_count> parameters{{
        {.attack_samples = 0, .decay_samples = 0, .sustain = 0.25f, .curve = 0.0f},
        {.attack_samples = 0, .decay_samples = 2, .sustain = 0.4f, .curve = 0.0f},
        {.attack_samples = 2, .decay_samples = 0, .sustain = 0.6f, .curve = 0.0f},
        {.attack_samples = 1, .decay_samples = 1, .sustain = 0.75f, .curve = 2.0f},
    }};

    for (std::size_t lane = 0; lane < lane_count; ++lane) {
        startLane(actual, lane, parameters[lane]);
        expected.startLane(lane, parameters[lane]);
    }
    requireState(actual, expected);
    REQUIRE(actual.attackMask().mask() == 0b1100);
    REQUIRE(actual.decayMask().mask() == 0b0010);
    REQUIRE(actual.sustainMask().mask() == 0b0001);

    REQUIRE(processAndRequire(actual, expected) == 0);
    REQUIRE(actual.attackMask().mask() == 0b0100);
    REQUIRE(actual.decayMask().mask() == 0b1010);
    REQUIRE(actual.sustainMask().mask() == 0b0001);

    REQUIRE(processAndRequire(actual, expected) == 0);
    REQUIRE(actual.sustainMask().mask() == lane_mask);

    REQUIRE(releaseLane(actual, 0, 0, 0.0f) == expected.releaseLane(0, 0, 0.0f));
    REQUIRE(releaseLane(actual, 1, 1, 0.0f) == expected.releaseLane(1, 1, 0.0f));
    REQUIRE(releaseLane(actual, 2, 2, 0.0f) == expected.releaseLane(2, 2, 0.0f));
    REQUIRE(releaseLane(actual, 3, 3, 0.0f) == expected.releaseLane(3, 3, 0.0f));
    requireState(actual, expected);
    REQUIRE(actual.releaseMask().mask() == 0b1110);

    REQUIRE(processAndRequire(actual, expected) == 0b0010);
    REQUIRE(processAndRequire(actual, expected) == 0b0100);
    REQUIRE(processAndRequire(actual, expected) == 0b1000);
    REQUIRE(processAndRequire(actual, expected) == 0);
    REQUIRE(actual.activeMask().mask() == 0);
}

TEST_CASE("SimdAdsr applies stage operations to lane masks", "[dsp][simd-adsr]") {
    SimdAdsr actual;
    AdsrOracle expected;
    const SimdAdsrSettings parameters{
        .attack_samples = 3,
        .decay_samples = 2,
        .sustain = 0.4f,
        .curve = -2.0f,
    };

    actual.start(SimdAdsr::Mask::from_mask(0b1011), parameters);
    for (const std::size_t lane : {0, 1, 3}) expected.startLane(lane, parameters);
    requireState(actual, expected);

    REQUIRE(processAndRequire(actual, expected) == 0);
    actual.kill(SimdAdsr::Mask::from_mask(0b0010));
    expected.killLane(1);
    requireState(actual, expected);

    const auto immediately_finished = actual.release(SimdAdsr::Mask::from_mask(0b1001), 3, 1.0f);
    REQUIRE(immediately_finished.mask() == 0);
    REQUIRE_FALSE(expected.releaseLane(0, 3, 1.0f));
    REQUIRE_FALSE(expected.releaseLane(3, 3, 1.0f));

    REQUIRE(processAndRequire(actual, expected) == 0);
    REQUIRE(processAndRequire(actual, expected) == 0);
    REQUIRE(processAndRequire(actual, expected) == 0b1001);
    REQUIRE(actual.activeMask().mask() == 0);
}

TEST_CASE("SimdAdsr curves match the double precision ADSR oracle", "[dsp][simd-adsr]") {
    constexpr std::array<float, 9> curves{
        -12.0f, -4.0f, -0.01f, -0.009f, 0.0f, 0.009f, 0.01f, 4.0f, 12.0f,
    };

    SimdAdsr actual;
    AdsrOracle expected;
    for (std::size_t index = 0; index < curves.size(); ++index) {
        CAPTURE(curves[index]);
        const std::size_t lane = index % lane_count;
        const MaskValue mask = MaskValue{1} << lane;
        const SimdAdsrSettings parameters{
            .attack_samples = 7,
            .decay_samples = 5,
            .release_samples = 6,
            .sustain = 0.37f,
            .curve = curves[index],
        };
        startLane(actual, lane, parameters);
        expected.startLane(lane, parameters);
        requireState(actual, expected);

        for (std::uint32_t sample = 0; sample < parameters.attack_samples + parameters.decay_samples; ++sample)
            REQUIRE(processAndRequire(actual, expected) == 0);
        REQUIRE(actual.sustainMask().mask() == mask);

        REQUIRE_FALSE(releaseLane(actual, lane, parameters.release_samples, parameters.curve));
        REQUIRE_FALSE(expected.releaseLane(lane, parameters.release_samples, parameters.curve));
        requireState(actual, expected);
        for (std::uint32_t sample = 0; sample < parameters.release_samples; ++sample) {
            const MaskValue finished = processAndRequire(actual, expected);
            REQUIRE(finished == (sample + 1 == parameters.release_samples ? mask : MaskValue{0}));
        }
        REQUIRE(processAndRequire(actual, expected) == 0);
    }
}

TEST_CASE("SimdAdsr release starts from each lane's current value", "[dsp][simd-adsr]") {
    SimdAdsr actual;
    AdsrOracle expected;
    const std::array<SimdAdsrSettings, lane_count> parameters{{
        {.attack_samples = 8, .decay_samples = 4, .sustain = 0.2f, .curve = -3.0f},
        {.attack_samples = 0, .decay_samples = 8, .sustain = 0.3f, .curve = 2.0f},
        {.attack_samples = 4, .decay_samples = 0, .sustain = 0.5f, .curve = 0.0f},
        {.attack_samples = 0, .decay_samples = 0, .sustain = 0.75f, .curve = 0.0f},
    }};
    for (std::size_t lane = 0; lane < lane_count; ++lane) {
        startLane(actual, lane, parameters[lane]);
        expected.startLane(lane, parameters[lane]);
    }
    for (int sample = 0; sample < 3; ++sample) REQUIRE(processAndRequire(actual, expected) == 0);

    const auto before_release = unpack(actual.value());
    REQUIRE_FALSE(releaseLane(actual, 0, 5, 4.0f));
    REQUIRE_FALSE(expected.releaseLane(0, 5, 4.0f));
    REQUIRE_FALSE(releaseLane(actual, 1, 7, -4.0f));
    REQUIRE_FALSE(expected.releaseLane(1, 7, -4.0f));
    const auto after_release = unpack(actual.value());
    REQUIRE(after_release[0] == before_release[0]);
    REQUIRE(after_release[1] == before_release[1]);

    killLane(actual, 2);
    expected.killLane(2);
    startLane(actual, 2, parameters[2]);
    expected.startLane(2, parameters[2]);
    REQUIRE(releaseLane(actual, 2, 4, 0.0f));
    REQUIRE(expected.releaseLane(2, 4, 0.0f));
    REQUIRE(releaseLane(actual, 3, 0, 0.0f));
    REQUIRE(expected.releaseLane(3, 0, 0.0f));
    requireState(actual, expected);

    for (int sample = 1; sample <= 7; ++sample) {
        const MaskValue finished = processAndRequire(actual, expected);
        REQUIRE(finished == (sample == 5 ? 0b0001 : sample == 7 ? 0b0010 : 0));
    }
    REQUIRE(processAndRequire(actual, expected) == 0);
}

TEST_CASE("SimdAdsr keeps divergent lane stages independent across call partitions", "[dsp][simd-adsr]") {
    const auto configure = [](SimdAdsr& actual, AdsrOracle& expected) {
        const std::array<SimdAdsrSettings, lane_count> parameters{{
            {.attack_samples = 9, .decay_samples = 4, .sustain = 0.2f, .curve = -2.0f},
            {.attack_samples = 0, .decay_samples = 11, .sustain = 0.4f, .curve = 3.0f},
            {.attack_samples = 0, .decay_samples = 0, .sustain = 0.6f, .curve = 0.0f},
            {.attack_samples = 0, .decay_samples = 0, .sustain = 0.8f, .curve = 0.0f},
        }};
        for (std::size_t lane = 0; lane < lane_count; ++lane) {
            startLane(actual, lane, parameters[lane]);
            expected.startLane(lane, parameters[lane]);
        }
        REQUIRE_FALSE(releaseLane(actual, 3, 7, -3.0f));
        REQUIRE_FALSE(expected.releaseLane(3, 7, -3.0f));
    };

    SimdAdsr contiguous;
    SimdAdsr partitioned;
    AdsrOracle contiguous_oracle;
    AdsrOracle partitioned_oracle;
    configure(contiguous, contiguous_oracle);
    configure(partitioned, partitioned_oracle);
    REQUIRE(contiguous.attackMask().mask() == 0b0001);
    REQUIRE(contiguous.decayMask().mask() == 0b0010);
    REQUIRE(contiguous.sustainMask().mask() == 0b0100);
    REQUIRE(contiguous.releaseMask().mask() == 0b1000);

    constexpr std::size_t sample_count = 13;
    std::array<std::array<float, lane_count>, sample_count> reference{};
    std::array<MaskValue, sample_count> reference_finished{};
    for (std::size_t sample = 0; sample < sample_count; ++sample) {
        reference_finished[sample] = processAndRequire(contiguous, contiguous_oracle);
        reference[sample] = unpack(contiguous.value());
    }

    constexpr std::array<std::size_t, 4> partitions{1, 3, 2, 7};
    std::size_t sample = 0;
    for (const auto partition : partitions) {
        for (std::size_t offset = 0; offset < partition; ++offset, ++sample) {
            REQUIRE(processAndRequire(partitioned, partitioned_oracle) == reference_finished[sample]);
            REQUIRE(unpack(partitioned.value()) == reference[sample]);
        }
    }
    REQUIRE(sample == sample_count);
    requireState(partitioned, contiguous_oracle);
}

TEST_CASE("SimdAdsr kill is isolated and a killed lane can be reused", "[dsp][simd-adsr]") {
    SimdAdsr actual;
    AdsrOracle expected;
    const SimdAdsrSettings first{
        .attack_samples = 10,
        .decay_samples = 7,
        .sustain = 0.25f,
        .curve = -2.0f,
    };
    const SimdAdsrSettings neighbor{
        .attack_samples = 8,
        .decay_samples = 3,
        .sustain = 0.5f,
        .curve = 2.0f,
    };
    startLane(actual, 0, first);
    expected.startLane(0, first);
    startLane(actual, 3, neighbor);
    expected.startLane(3, neighbor);
    REQUIRE(processAndRequire(actual, expected) == 0);
    REQUIRE(processAndRequire(actual, expected) == 0);

    const float neighbor_before_kill = unpack(actual.value())[3];
    killLane(actual, 0);
    expected.killLane(0);
    killLane(actual, 0);
    expected.killLane(0);
    REQUIRE(unpack(actual.value())[3] == neighbor_before_kill);
    requireState(actual, expected);

    const SimdAdsrSettings sustain{
        .attack_samples = 0,
        .decay_samples = 0,
        .sustain = 0.9f,
        .curve = 0.0f,
    };
    startLane(actual, 0, sustain);
    expected.startLane(0, sustain);
    requireState(actual, expected);
    REQUIRE(actual.sustainMask().mask() == 0b0001);

    killLane(actual, 0);
    expected.killLane(0);
    const SimdAdsrSettings reused{
        .attack_samples = 3,
        .decay_samples = 2,
        .sustain = 0.4f,
        .curve = 3.0f,
    };
    startLane(actual, 0, reused);
    expected.startLane(0, reused);
    REQUIRE_FALSE(releaseLane(actual, 3, 5, -3.0f));
    REQUIRE_FALSE(expected.releaseLane(3, 5, -3.0f));
    REQUIRE(processAndRequire(actual, expected) == 0);
    REQUIRE(processAndRequire(actual, expected) == 0);
    killLane(actual, 3);
    expected.killLane(3);

    for (int sample = 0; sample < 4; ++sample) REQUIRE((processAndRequire(actual, expected) & 0b1000) == 0);
    REQUIRE(actual.sustainMask().mask() == 0b0001);
    killLane(actual, 0);
    expected.killLane(0);
    requireState(actual, expected);
}
