#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <applause/core/ModMatrix.h>
#include <applause/extensions/ParamsExtension.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace applause;

constexpr ModMatrix::Config SmallConfig{4, 8, 16, 32};
constexpr ModMatrix::Config StandardConfig{16, 32, 64, 128};
constexpr ModMatrix::Config MinimalConfig{1, 1, 1, 1};

// Naive reference implementation of the modulation matrix, used by the K-series
// oracle tests to cross-check ModMatrix output.
class ModMatrixOracle {
public:
    struct Source {
        uint16_t index;
        bool is_mono;
        bool is_both;
        bool bipolar;
        bool current_mode_is_mono;
    };

    struct Destination {
        uint16_t index;
        bool is_mono;
        float base_value = 0.0f;
        applause::ValueScaleInfo scale_info = {0.0f, 1.0f, applause::ValueScaling::linear()};
    };

    struct Connection {
        uint16_t src_idx;
        uint16_t target;       // dst_idx for param connections, target_slot for depth mods
        uint16_t depth_slot;
        bool is_depth_mod;
        bool bipolar_mapping;
    };

    std::vector<Source> sources;
    std::vector<Destination> destinations;
    std::vector<Connection> connections;
    std::vector<float> mono_src_values;
    std::vector<std::vector<float>> poly_src_values;
    std::vector<float> depth_base;
    std::vector<uint16_t> active_voices;

    uint16_t num_voices;
    uint16_t max_sources;

    ModMatrixOracle(uint16_t num_voices_, uint16_t max_sources_, uint16_t max_destinations_)
        : num_voices(num_voices_), max_sources(max_sources_)
    {
        mono_src_values.resize(max_sources_, 0.0f);
        poly_src_values.resize(num_voices_);
        for (auto& v : poly_src_values) {
            v.resize(max_sources_, 0.0f);
        }
    }

    uint16_t addSource(bool is_mono, bool is_both, bool bipolar) {
        uint16_t idx = static_cast<uint16_t>(sources.size());
        sources.push_back({idx, is_mono, is_both, bipolar, is_mono});
        return idx;
    }

    uint16_t addDestination(bool is_mono, applause::ValueScaleInfo scale = {0.0f, 1.0f, applause::ValueScaling::linear()}) {
        uint16_t idx = static_cast<uint16_t>(destinations.size());
        destinations.push_back({idx, is_mono, 0.0f, scale});
        return idx;
    }

    uint16_t addConnection(uint16_t src, uint16_t dst, float depth, bool bipolar_mapping) {
        uint16_t slot = static_cast<uint16_t>(depth_base.size());
        depth_base.push_back(depth);
        connections.push_back({src, dst, slot, false, bipolar_mapping});
        return slot;
    }

    uint16_t addDepthModulation(uint16_t src, uint16_t target_slot, float depth, bool bipolar_mapping) {
        uint16_t slot = static_cast<uint16_t>(depth_base.size());
        depth_base.push_back(depth);
        connections.push_back({src, target_slot, slot, true, bipolar_mapping});
        return slot;
    }

    void setMonoSource(uint16_t src, float value) {
        mono_src_values[src] = value;
    }

    void setPolySource(uint16_t src, uint16_t voice, float value) {
        poly_src_values[voice][src] = value;
    }

    void setBaseValue(uint16_t dst, float norm_value) {
        destinations[dst].base_value = norm_value;
    }

    void setSourceMode(uint16_t src, bool is_mono) {
        sources[src].current_mode_is_mono = is_mono;
    }

    bool effectivelyMono(uint16_t src) const {
        const auto& s = sources[src];
        if (s.is_both) return s.current_mode_is_mono;
        return s.is_mono;
    }

    // Serum-style peak-to-peak normalization: a bipolar-mapped connection
    // contributes offsets in [-0.5, +0.5] so `depth` equals peak-to-peak swing.
    static float applyBipolarNormalization(float src_val, bool src_bipolar, bool bipolar_mapping) {
        if (src_bipolar)
            src_val = (src_val + 1.0f) * 0.5f;
        if (bipolar_mapping)
            src_val -= 0.5f;
        return src_val;
    }

    std::pair<std::vector<float>, std::vector<std::vector<float>>> process() {
        std::vector<float> mono_out(destinations.size());
        std::vector<std::vector<float>> poly_out(num_voices);

        for (size_t d = 0; d < destinations.size(); ++d) {
            mono_out[d] = destinations[d].base_value;
        }

        for (uint16_t v = 0; v < num_voices; ++v) {
            poly_out[v].resize(destinations.size());
            for (size_t d = 0; d < destinations.size(); ++d) {
                poly_out[v][d] = destinations[d].base_value;
            }
        }

        std::vector<float> mono_depth = depth_base;
        std::vector<std::vector<float>> poly_depth(num_voices);
        for (auto& pd : poly_depth)
            pd = depth_base;

        for (const auto& conn : connections) {
            if (!conn.is_depth_mod) continue;
            if (effectivelyMono(conn.src_idx)) {
                float src_val = mono_src_values[conn.src_idx];
                src_val = applyBipolarNormalization(src_val, sources[conn.src_idx].bipolar, conn.bipolar_mapping);
                float depth = depth_base[conn.depth_slot];
                mono_depth[conn.target] += src_val * depth;
            }
        }

        for (uint16_t v : active_voices) {
            poly_depth[v] = mono_depth;
            for (const auto& conn : connections) {
                if (!conn.is_depth_mod) continue;
                if (!effectivelyMono(conn.src_idx)) {
                    float src_val = poly_src_values[v][conn.src_idx];
                    src_val = applyBipolarNormalization(src_val, sources[conn.src_idx].bipolar, conn.bipolar_mapping);
                    float depth = depth_base[conn.depth_slot];
                    poly_depth[v][conn.target] += src_val * depth;
                }
            }
        }

        for (const auto& conn : connections) {
            if (conn.is_depth_mod) continue;

            bool src_mono = effectivelyMono(conn.src_idx);
            bool dst_mono = destinations[conn.target].is_mono;
            bool src_bipolar = sources[conn.src_idx].bipolar;

            if (src_mono && dst_mono) {
                float src_val = mono_src_values[conn.src_idx];
                src_val = applyBipolarNormalization(src_val, src_bipolar, conn.bipolar_mapping);
                mono_out[conn.target] += src_val * mono_depth[conn.depth_slot];
            }
            else if (src_mono && !dst_mono) {
                for (uint16_t v : active_voices) {
                    float src_val = mono_src_values[conn.src_idx];
                    src_val = applyBipolarNormalization(src_val, src_bipolar, conn.bipolar_mapping);
                    poly_out[v][conn.target] += src_val * poly_depth[v][conn.depth_slot];
                }
            }
            else if (!src_mono && !dst_mono) {
                for (uint16_t v : active_voices) {
                    float src_val = poly_src_values[v][conn.src_idx];
                    src_val = applyBipolarNormalization(src_val, src_bipolar, conn.bipolar_mapping);
                    poly_out[v][conn.target] += src_val * poly_depth[v][conn.depth_slot];
                }
            }
            // poly src -> mono dst: NYI in ModMatrix, so oracle leaves it out too.
        }

        for (size_t d = 0; d < destinations.size(); ++d) {
            const auto& scale = destinations[d].scale_info;
            float norm = std::clamp(mono_out[d], 0.0f, 1.0f);
            mono_out[d] = scale.scaling.fromNormalized(norm, scale.min, scale.max);

            if (!destinations[d].is_mono) {
                for (uint16_t v : active_voices) {
                    norm = std::clamp(poly_out[v][d], 0.0f, 1.0f);
                    poly_out[v][d] = scale.scaling.fromNormalized(norm, scale.min, scale.max);
                }
            }
        }

        return {mono_out, poly_out};
    }
};

TEST_CASE("A1: Registering sources assigns stable indices and stores flags", "[modmatrix][registration]")
{
    ModMatrix matrix(SmallConfig);

    auto& lfo1 = matrix.registerSource("LFO1", ModSrcType::Both, true, ModSrcMode::Mono);
    auto& env1 = matrix.registerSource("ENV1", ModSrcType::Poly, false);
    auto& macro1 = matrix.registerSource("MACRO1", ModSrcType::Mono, false);

    SECTION("Indices are unique and contiguous from 0") {
        REQUIRE(lfo1.index == 0);
        REQUIRE(env1.index == 1);
        REQUIRE(macro1.index == 2);
    }

    SECTION("Both-type source uses defaultMode") {
        REQUIRE(lfo1.type == ModSrcType::Both);
        REQUIRE(lfo1.mode == ModSrcMode::Mono);
        REQUIRE(lfo1.bipolar == true);
    }

    SECTION("Poly-type source always has Poly mode") {
        REQUIRE(env1.type == ModSrcType::Poly);
        REQUIRE(env1.mode == ModSrcMode::Poly);
        REQUIRE(env1.bipolar == false);
    }

    SECTION("Mono-type source always has Mono mode") {
        REQUIRE(macro1.type == ModSrcType::Mono);
        REQUIRE(macro1.mode == ModSrcMode::Mono);
        REQUIRE(macro1.bipolar == false);
    }
}

TEST_CASE("A3: Registering destinations stores mode and poly index list", "[modmatrix][registration]")
{
    ModMatrix matrix(SmallConfig);

    applause::ValueScaleInfo cutoff_scale{20.0f, 20000.0f, applause::ValueScaling::linear()};
    auto& cutoff = matrix.registerDestination("Cutoff", ModDstMode::Poly, cutoff_scale);
    auto& gain = matrix.registerDestination("Gain", ModDstMode::Mono);

    SECTION("Destination indices are contiguous") {
        REQUIRE(cutoff.index == 0);
        REQUIRE(gain.index == 1);
    }

    SECTION("Destination modes are stored correctly") {
        REQUIRE(cutoff.mode == ModDstMode::Poly);
        REQUIRE(gain.mode == ModDstMode::Mono);
    }

    SECTION("Poly destination is processed for active voices") {
        auto& src = matrix.registerSource("src", ModSrcType::Mono);
        matrix.addConnection(src, cutoff, 0.5f);
        matrix.setBaseValue(cutoff.index, 1000.0f);
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.notifyVoiceOn(0);
        matrix.process();

        REQUIRE(matrix.getPolyModValue(cutoff.index, 0) > 1000.0f);
    }
}

TEST_CASE("B1: Voice on adds voice once (no duplicates)", "[modmatrix][voice]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Poly);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);
    matrix.addConnection(src, dst, 1.0f, false);
    matrix.setBaseValue(dst.index, 0.0f);

    matrix.notifyVoiceOn(2);
    matrix.notifyVoiceOn(2);
    matrix.notifyVoiceOn(2);

    matrix.setPolySourceValue(src.index, 2, 0.5f);
    matrix.process();

    // Duplicate notifyVoiceOn calls must not stack -- output stays 0 + 0.5 * 1.0 = 0.5.
    REQUIRE(matrix.getPolyModValue(dst.index, 2) == Catch::Approx(0.5f));
}

TEST_CASE("B2: Voice off removes voice from processing", "[modmatrix][voice]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Poly);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);
    matrix.addConnection(src, dst, 1.0f, false);

    matrix.notifyVoiceOn(1);
    matrix.setBaseValue(dst.index, 0.5f);
    matrix.setPolySourceValue(src.index, 1, 0.3f);
    matrix.process();

    float before_off = matrix.getPolyModValue(dst.index, 1);
    REQUIRE(before_off == Catch::Approx(0.8f));

    matrix.notifyVoiceOff(1);
    matrix.setPolySourceValue(src.index, 1, 1.0f);
    matrix.process();

    // Inactive voices are not touched, so the buffer retains its last computed value.
    REQUIRE(matrix.getPolyModValue(dst.index, 1) == Catch::Approx(before_off));
}

TEST_CASE("B3: notifyVoiceOff on inactive voice is safe", "[modmatrix][voice]")
{
    ModMatrix matrix(SmallConfig);

    matrix.notifyVoiceOff(0);
    matrix.notifyVoiceOff(1);
    matrix.notifyVoiceOff(3);
}

TEST_CASE("C1: With no connections, output equals base plain value after scaling", "[modmatrix][scaling]")
{
    ModMatrix matrix(SmallConfig);

    SECTION("Mono destination") {
        applause::ValueScaleInfo scale{0.0f, 100.0f, applause::ValueScaling::linear()};
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, scale);

        matrix.setBaseValue(dst.index, 25.0f);
        matrix.process();

        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(25.0f));
    }

    SECTION("Poly destination with active voice") {
        applause::ValueScaleInfo scale{0.0f, 100.0f, applause::ValueScaling::linear()};
        auto& dst = matrix.registerDestination("dst", ModDstMode::Poly, scale);

        matrix.setBaseValue(dst.index, 75.0f);
        matrix.notifyVoiceOn(2);
        matrix.process();

        REQUIRE(matrix.getPolyModValue(dst.index, 2) == Catch::Approx(75.0f));
    }

    SECTION("Identity scaling (0..1)") {
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);
        matrix.setBaseValue(dst.index, 0.5f);
        matrix.process();

        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));
    }
}

TEST_CASE("C2: Clamping behavior - normalized outside [0,1] clamps to min/max", "[modmatrix][scaling]")
{
    ModMatrix matrix(SmallConfig);

    applause::ValueScaleInfo scale{0.0f, 100.0f, applause::ValueScaling::linear()};
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, scale);

    SECTION("Modulation pushing below 0 clamps to min") {
        matrix.setBaseValue(dst.index, 10.0f);
        matrix.addConnection(src, dst, -0.5f, false);
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();

        // 0.1 + 1.0 * -0.5 = -0.4, clamps to 0 -> 0.0 plain.
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));
    }

    SECTION("Modulation pushing above 1 clamps to max") {
        matrix.setBaseValue(dst.index, 90.0f);
        matrix.addConnection(src, dst, 0.5f, false);
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();

        // 0.9 + 1.0 * 0.5 = 1.4, clamps to 1 -> 100.0 plain.
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(100.0f));
    }
}

TEST_CASE("C4: loadParamBaseValues with extra destinations", "[modmatrix][scaling]")
{
    // TODO: loadParamBaseValues() loops to dst_count_, which exceeds the param count when
    // extra destinations are registered after registerFromParamsExtension(). Should track
    // num_param_dests_ separately. This test pins the current safe-with-N-params behavior.

    ModMatrix matrix(SmallConfig);

    applause::ParamsExtension params(8);

    applause::ParamConfig config1;
    config1.string_id = "param1";
    config1.name = "Param 1";
    config1.min_value = 0.0f;
    config1.max_value = 1.0f;
    config1.default_value = 0.5f;
    config1.is_polyphonic = false;
    config1.scaling = applause::ValueScaling::linear();
    params.registerParam(config1);

    applause::ParamConfig config2;
    config2.string_id = "param2";
    config2.name = "Param 2";
    config2.min_value = 0.0f;
    config2.max_value = 100.0f;
    config2.default_value = 50.0f;
    config2.is_polyphonic = false;
    config2.scaling = applause::ValueScaling::linear();
    params.registerParam(config2);

    matrix.registerFromParamsExtension(params);
    matrix.registerDestination("extra", ModDstMode::Mono);

    matrix.loadParamBaseValues(params);
    matrix.process();

    REQUIRE(matrix.getModValue(0) == Catch::Approx(0.5f));
    REQUIRE(matrix.getModValue(1) == Catch::Approx(50.0f));
}

TEST_CASE("D1: Mono source values propagate through MM connections", "[modmatrix][sources]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("MACRO1", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    matrix.addConnection(src, dst, 0.5f, false);
    matrix.setBaseValue(dst.index, 0.25f);

    SECTION("Source value 0.0") {
        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.25f));
    }

    SECTION("Source value 0.5") {
        matrix.setMonoSourceValue(src.index, 0.5f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));
    }

    SECTION("Source value 1.0") {
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.75f));
    }
}

TEST_CASE("D2: Poly source values are per-voice and only active voices processed", "[modmatrix][sources]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("ENV1", ModSrcType::Poly, false);
    auto& dst = matrix.registerDestination("Cutoff", ModDstMode::Poly);

    matrix.addConnection(src, dst, 1.0f, false);
    matrix.setBaseValue(dst.index, 0.0f);

    matrix.setPolySourceValue(src.index, 0, 0.0f);
    matrix.setPolySourceValue(src.index, 1, 0.5f);
    matrix.setPolySourceValue(src.index, 2, 1.0f);
    matrix.setPolySourceValue(src.index, 3, 0.25f);

    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(2);

    matrix.process();

    SECTION("Active voice 0 reflects its source value") {
        REQUIRE(matrix.getPolyModValue(dst.index, 0) == Catch::Approx(0.0f));
    }

    SECTION("Active voice 2 reflects its source value") {
        REQUIRE(matrix.getPolyModValue(dst.index, 2) == Catch::Approx(1.0f));
    }
}

TEST_CASE("E1: Four mapping combinations for main connections", "[modmatrix][mapping]")
{
    applause::ValueScaleInfo identity_scale{0.0f, 1.0f, applause::ValueScaling::linear()};

    SECTION("bipolar src, bipolar mapping (peak-to-peak = d, centered)") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, true);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);
        matrix.addConnection(src, dst, 1.0f, true);
        matrix.setBaseValue(dst.index, 0.5f);

        matrix.setMonoSourceValue(src.index, -1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("bipolar src, unipolar mapping (half-wave rectify)") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, true);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);
        matrix.addConnection(src, dst, 1.0f, false);
        matrix.setBaseValue(dst.index, 0.0f);

        matrix.setMonoSourceValue(src.index, -1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("unipolar src, bipolar mapping (center on midpoint)") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);
        matrix.addConnection(src, dst, 1.0f, true);
        matrix.setBaseValue(dst.index, 0.5f);

        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        matrix.setMonoSourceValue(src.index, 0.5f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("unipolar src, unipolar mapping (identity)") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);
        matrix.addConnection(src, dst, 1.0f, false);
        matrix.setBaseValue(dst.index, 0.0f);

        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        matrix.setMonoSourceValue(src.index, 0.5f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }
}

TEST_CASE("E2: Four mapping combinations for depth modulation", "[modmatrix][mapping]")
{
    applause::ValueScaleInfo identity_scale{0.0f, 1.0f, applause::ValueScaling::linear()};

    SECTION("bipolar depth src, bipolar mapping") {
        ModMatrix matrix(SmallConfig);
        auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
        auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, true);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);

        auto conn = matrix.addConnection(main_src, dst, 0.0f, false);
        matrix.addDepthModulation(depth_src, conn, 1.0f, true);
        matrix.setBaseValue(dst.index, 0.5f);
        matrix.setMonoSourceValue(main_src.index, 1.0f);

        matrix.setMonoSourceValue(depth_src.index, -1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        matrix.setMonoSourceValue(depth_src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        matrix.setMonoSourceValue(depth_src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("unipolar depth src, unipolar mapping (identity)") {
        ModMatrix matrix(SmallConfig);
        auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
        auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);

        auto conn = matrix.addConnection(main_src, dst, 0.0f, false);
        matrix.addDepthModulation(depth_src, conn, 1.0f, false);
        matrix.setBaseValue(dst.index, 0.0f);
        matrix.setMonoSourceValue(main_src.index, 1.0f);

        matrix.setMonoSourceValue(depth_src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        matrix.setMonoSourceValue(depth_src.index, 0.5f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        matrix.setMonoSourceValue(depth_src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }
}

TEST_CASE("F1: addConnection creates depth slot and stores base depth", "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(src, dst, 0.25f, false);

    SECTION("Connection depth slot is valid") {
        REQUIRE(conn.depth_slot == 0);
    }

    SECTION("Processing with source=1 and base=0 gives expected output") {
        matrix.setBaseValue(dst.index, 0.0f);
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();

        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.25f));
    }
}

TEST_CASE("F2: Adding same S->D again updates existing connection", "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn1 = matrix.addConnection(src, dst, 0.25f, false);
    auto conn2 = matrix.addConnection(src, dst, 0.75f, false);

    SECTION("Same slot is reused") {
        REQUIRE(conn1.depth_slot == conn2.depth_slot);
    }

    SECTION("Depth is updated to new value") {
        matrix.setBaseValue(dst.index, 0.0f);
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();

        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.75f));
    }
}

TEST_CASE("F2b: Adding same depth mod again updates existing depth mod connection", "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, dst, 0.0f, false);

    auto depth_conn1 = matrix.addDepthModulation(depth_src, conn, 0.25f, false);
    auto depth_conn2 = matrix.addDepthModulation(depth_src, conn, 0.75f, false);

    SECTION("Connection count remains 2 (no duplicate added)") {
        REQUIRE(matrix.getConnections().size() == 2);
    }

    SECTION("Same depth slot is reused") {
        REQUIRE(depth_conn1.depth_slot == depth_conn2.depth_slot);
    }

    SECTION("Depth is updated to new value") {
        matrix.setBaseValue(dst.index, 0.0f);
        matrix.setMonoSourceValue(main_src.index, 1.0f);
        matrix.setMonoSourceValue(depth_src.index, 1.0f);
        matrix.process();

        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.75f));
    }

    SECTION("Bipolar mapping flag is updated") {
        matrix.addDepthModulation(depth_src, conn, 0.5f, true);
        REQUIRE(matrix.getConnections().size() == 2);
        const auto& depth_mod = matrix.getConnections()[1];
        REQUIRE(depth_mod.isDepthMod());
        REQUIRE(depth_mod.isBipolar());
    }
}

TEST_CASE("F3: Multiple depth mod routes to same depth slot sum", "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& mod1 = matrix.registerSource("mod1", ModSrcType::Mono, false);
    auto& mod2 = matrix.registerSource("mod2", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, dst, 0.0f, false);
    matrix.addDepthModulation(mod1, conn, 0.1f, false);
    matrix.addDepthModulation(mod2, conn, 0.2f, false);

    matrix.setBaseValue(dst.index, 0.0f);
    matrix.setMonoSourceValue(main_src.index, 1.0f);
    matrix.setMonoSourceValue(mod1.index, 1.0f);
    matrix.setMonoSourceValue(mod2.index, 0.5f);

    matrix.process();

    // effective depth = 1.0 * 0.1 + 0.5 * 0.2 = 0.2
    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.2f));
}

TEST_CASE("F4: Depth modulation affects all connection types using that slot", "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& mono_src = matrix.registerSource("mono_src", ModSrcType::Mono, false);
    auto& poly_src = matrix.registerSource("poly_src", ModSrcType::Poly, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);
    auto& mono_dst = matrix.registerDestination("mono_dst", ModDstMode::Mono);
    auto& poly_dst = matrix.registerDestination("poly_dst", ModDstMode::Poly);

    auto mm_conn = matrix.addConnection(mono_src, mono_dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, mm_conn, 1.0f, false);

    auto mp_conn = matrix.addConnection(mono_src, poly_dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, mp_conn, 1.0f, false);

    auto pp_conn = matrix.addConnection(poly_src, poly_dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, pp_conn, 1.0f, false);

    matrix.setBaseValue(mono_dst.index, 0.0f);
    matrix.setBaseValue(poly_dst.index, 0.0f);
    matrix.setMonoSourceValue(mono_src.index, 1.0f);
    matrix.setMonoSourceValue(depth_src.index, 0.5f);
    matrix.setPolySourceValue(poly_src.index, 0, 1.0f);
    matrix.notifyVoiceOn(0);

    matrix.process();

    SECTION("MM connection affected by depth mod") {
        REQUIRE(matrix.getModValue(mono_dst.index) == Catch::Approx(0.5f));
    }

    SECTION("MP connection affected by depth mod") {
        // poly_dst gets MP (0.5) + PP (0.5) = 1.0.
        REQUIRE(matrix.getPolyModValue(poly_dst.index, 0) == Catch::Approx(1.0f));
    }
}

TEST_CASE("G1: Both-source toggling moves routes between buckets", "[modmatrix][toggle]")
{
    ModMatrix matrix(SmallConfig);

    auto& lfo = matrix.registerSource("LFO1", ModSrcType::Both, true, ModSrcMode::Mono);
    auto& cutoff = matrix.registerDestination("Cutoff", ModDstMode::Poly);

    matrix.addConnection(lfo, cutoff, 1.0f, true);
    matrix.setBaseValue(cutoff.index, 0.5f);

    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(1);

    SECTION("Mono mode: same modulation for all voices") {
        matrix.setMonoSourceValue(lfo.index, 0.5f);
        matrix.process();

        float v0 = matrix.getPolyModValue(cutoff.index, 0);
        float v1 = matrix.getPolyModValue(cutoff.index, 1);
        REQUIRE(v0 == Catch::Approx(v1));
    }

    SECTION("Poly mode: different modulation per voice") {
        matrix.setSourceMode(lfo.index, ModSrcMode::Poly);

        matrix.setPolySourceValue(lfo.index, 0, -1.0f);
        matrix.setPolySourceValue(lfo.index, 1, 1.0f);
        matrix.process();

        float v0 = matrix.getPolyModValue(cutoff.index, 0);
        float v1 = matrix.getPolyModValue(cutoff.index, 1);
        REQUIRE(v0 != Catch::Approx(v1));
    }
}

TEST_CASE("G2: Toggling source mode reclassifies depth-mod routes", "[modmatrix][toggle]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Both, false, ModSrcMode::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);

    auto conn = matrix.addConnection(main_src, dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, conn, 1.0f, false);

    matrix.setBaseValue(dst.index, 0.0f);
    matrix.setMonoSourceValue(main_src.index, 1.0f);
    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(1);

    SECTION("Mono mode: depth same for all voices") {
        matrix.setMonoSourceValue(depth_src.index, 0.5f);
        matrix.process();

        float v0 = matrix.getPolyModValue(dst.index, 0);
        float v1 = matrix.getPolyModValue(dst.index, 1);
        REQUIRE(v0 == Catch::Approx(v1));
        REQUIRE(v0 == Catch::Approx(0.5f));
    }

    SECTION("Poly mode: depth varies per voice") {
        matrix.setSourceMode(depth_src.index, ModSrcMode::Poly);
        matrix.setPolySourceValue(depth_src.index, 0, 0.2f);
        matrix.setPolySourceValue(depth_src.index, 1, 0.8f);
        matrix.process();

        REQUIRE(matrix.getPolyModValue(dst.index, 0) == Catch::Approx(0.2f));
        REQUIRE(matrix.getPolyModValue(dst.index, 1) == Catch::Approx(0.8f));
    }
}

TEST_CASE("G3: Dynamic mode toggle during execution", "[modmatrix][toggle]")
{
    ModMatrix matrix(SmallConfig);

    auto& lfo = matrix.registerSource("LFO1", ModSrcType::Both, true, ModSrcMode::Mono);
    auto& cutoff = matrix.registerDestination("Cutoff", ModDstMode::Poly);

    matrix.addConnection(lfo, cutoff, 1.0f, true);
    matrix.setBaseValue(cutoff.index, 0.5f);

    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(1);

    // Both mono and poly values are populated upfront so the toggle is the only thing changing.
    matrix.setMonoSourceValue(lfo.index, 0.0f);
    matrix.setPolySourceValue(lfo.index, 0, -1.0f);
    matrix.setPolySourceValue(lfo.index, 1, 1.0f);

    matrix.process();
    float v0_mono = matrix.getPolyModValue(cutoff.index, 0);
    float v1_mono = matrix.getPolyModValue(cutoff.index, 1);
    REQUIRE(v0_mono == Catch::Approx(v1_mono));

    matrix.setSourceMode(lfo.index, ModSrcMode::Poly);
    matrix.process();
    float v0_poly = matrix.getPolyModValue(cutoff.index, 0);
    float v1_poly = matrix.getPolyModValue(cutoff.index, 1);
    REQUIRE(v0_poly != Catch::Approx(v1_poly));

    matrix.setSourceMode(lfo.index, ModSrcMode::Mono);
    matrix.process();
    float v0_back = matrix.getPolyModValue(cutoff.index, 0);
    float v1_back = matrix.getPolyModValue(cutoff.index, 1);
    REQUIRE(v0_back == Catch::Approx(v1_back));
    REQUIRE(v0_back == Catch::Approx(v0_mono));
}

TEST_CASE("H1: Outputs do not accumulate across blocks", "[modmatrix][determinism]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    matrix.addConnection(src, dst, 0.5f, false);
    matrix.setBaseValue(dst.index, 0.0f);
    matrix.setMonoSourceValue(src.index, 1.0f);

    matrix.process();
    float first = matrix.getModValue(dst.index);

    matrix.process();
    float second = matrix.getModValue(dst.index);

    REQUIRE(first == Catch::Approx(0.5f));
    REQUIRE(second == Catch::Approx(first));
}

TEST_CASE("H2: Order independence within a bucket (commutativity)", "[modmatrix][determinism]")
{
    ModMatrix matrix(SmallConfig);

    auto& src1 = matrix.registerSource("src1", ModSrcType::Mono, false);
    auto& src2 = matrix.registerSource("src2", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    matrix.addConnection(src1, dst, 0.3f, false);
    matrix.addConnection(src2, dst, 0.2f, false);

    matrix.setBaseValue(dst.index, 0.0f);
    matrix.setMonoSourceValue(src1.index, 1.0f);
    matrix.setMonoSourceValue(src2.index, 1.0f);

    matrix.process();
    float result = matrix.getModValue(dst.index);
    REQUIRE(result == Catch::Approx(0.5f));

    ModMatrix matrix2(SmallConfig);
    auto& src2b = matrix2.registerSource("src2", ModSrcType::Mono, false);
    auto& src1b = matrix2.registerSource("src1", ModSrcType::Mono, false);
    auto& dstb = matrix2.registerDestination("dst", ModDstMode::Mono);

    matrix2.addConnection(src2b, dstb, 0.2f, false);
    matrix2.addConnection(src1b, dstb, 0.3f, false);

    matrix2.setBaseValue(dstb.index, 0.0f);
    matrix2.setMonoSourceValue(src1b.index, 1.0f);
    matrix2.setMonoSourceValue(src2b.index, 1.0f);

    matrix2.process();
    REQUIRE(matrix2.getModValue(dstb.index) == Catch::Approx(result));
}

TEST_CASE("I1: Handle points to correct value for mono dest", "[modmatrix][handle]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    matrix.addConnection(src, dst, 0.5f, false);
    matrix.setBaseValue(dst.index, 0.25f);
    matrix.setMonoSourceValue(src.index, 0.5f);

    auto handle = matrix.getModHandle(dst.index);
    matrix.process();

    REQUIRE(handle.getValue() == Catch::Approx(matrix.getModValue(dst.index)));
    REQUIRE(handle.getValue() == Catch::Approx(0.5f));
}

TEST_CASE("I2: Handle points to correct per-voice value for poly dest", "[modmatrix][handle]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Poly, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);

    matrix.addConnection(src, dst, 1.0f, false);
    matrix.setBaseValue(dst.index, 0.0f);

    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(2);

    matrix.setPolySourceValue(src.index, 0, 0.3f);
    matrix.setPolySourceValue(src.index, 2, 0.7f);

    auto handle0 = matrix.getModHandle(dst.index, 0);
    auto handle2 = matrix.getModHandle(dst.index, 2);

    matrix.process();

    REQUIRE(handle0.getValue() == Catch::Approx(matrix.getPolyModValue(dst.index, 0)));
    REQUIRE(handle2.getValue() == Catch::Approx(matrix.getPolyModValue(dst.index, 2)));
    REQUIRE(handle0.getValue() == Catch::Approx(0.3f));
    REQUIRE(handle2.getValue() == Catch::Approx(0.7f));
}

TEST_CASE("J1: Poly->Mono connections are compiled but have no effect (NYI)", "[modmatrix][nyi]")
{
    ModMatrix matrix(SmallConfig);

    auto& poly_src = matrix.registerSource("poly_src", ModSrcType::Poly, false);
    auto& mono_dst = matrix.registerDestination("mono_dst", ModDstMode::Mono);

    matrix.addConnection(poly_src, mono_dst, 1.0f, false);
    matrix.setBaseValue(mono_dst.index, 0.5f);

    matrix.notifyVoiceOn(0);
    matrix.setPolySourceValue(poly_src.index, 0, 1.0f);

    matrix.process();

    // PM is compiled but its processing loop is a no-op, so the destination keeps its base value.
    REQUIRE(matrix.getModValue(mono_dst.index) == Catch::Approx(0.5f));
}

TEST_CASE("J2: Poly depth mod on MM slot is silently ignored", "[modmatrix][nyi]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Poly, false);
    auto& mono_dst = matrix.registerDestination("mono_dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, mono_dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, conn, 1.0f, false);

    matrix.setBaseValue(mono_dst.index, 0.0f);
    matrix.setMonoSourceValue(main_src.index, 1.0f);

    matrix.notifyVoiceOn(0);
    matrix.setPolySourceValue(depth_src.index, 0, 0.8f);

    matrix.process();

    // MM connections read mono_depth_buf_; poly depth mods write to poly_depth_buf_,
    // so the MM path sees only the base depth (0.0).
    REQUIRE(matrix.getModValue(mono_dst.index) == Catch::Approx(0.0f));
}

TEST_CASE("Edge: Empty matrix operations", "[modmatrix][edge]")
{
    ModMatrix matrix(SmallConfig);

    SECTION("process() with no sources/destinations doesn't crash") {
        matrix.process();
    }

    SECTION("process() with sources but no destinations") {
        matrix.registerSource("src", ModSrcType::Mono);
        matrix.setMonoSourceValue(0, 0.5f);
        matrix.process();
    }

    SECTION("process() with destinations but no sources") {
        matrix.registerDestination("dst", ModDstMode::Mono);
        matrix.process();
        REQUIRE(matrix.getModValue(0) == Catch::Approx(0.0f));
    }

    SECTION("process() with sources and destinations but no connections") {
        matrix.registerSource("src", ModSrcType::Mono);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);
        matrix.setBaseValue(dst.index, 0.5f);
        matrix.setMonoSourceValue(0, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(0) == Catch::Approx(0.5f));
    }
}

TEST_CASE("Edge: Connections with no active voices", "[modmatrix][edge]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("poly_src", ModSrcType::Poly, false);
    auto& dst = matrix.registerDestination("poly_dst", ModDstMode::Poly);

    matrix.addConnection(src, dst, 1.0f, false);
    matrix.setBaseValue(dst.index, 0.5f);
    matrix.setPolySourceValue(src.index, 0, 0.8f);

    matrix.process();
}

TEST_CASE("Edge: Negative depth values invert modulation", "[modmatrix][edge]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    matrix.addConnection(src, dst, -0.5f, false);
    matrix.setBaseValue(dst.index, 0.5f);
    matrix.setMonoSourceValue(src.index, 1.0f);

    matrix.process();

    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));
}

TEST_CASE("Edge: Zero base depth with depth modulation", "[modmatrix][edge]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, conn, 1.0f, false);

    matrix.setBaseValue(dst.index, 0.0f);
    matrix.setMonoSourceValue(main_src.index, 1.0f);
    matrix.setMonoSourceValue(depth_src.index, 0.75f);

    matrix.process();

    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.75f));
}

TEST_CASE("K1: Oracle verification for simple mono patch", "[modmatrix][oracle]")
{
    ModMatrix matrix(SmallConfig);
    ModMatrixOracle oracle(4, 8, 16);

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);
    oracle.addSource(true, false, false);
    oracle.addDestination(true);

    matrix.addConnection(src, dst, 0.5f, false);
    oracle.addConnection(0, 0, 0.5f, false);

    matrix.setBaseValue(dst.index, 0.25f);
    oracle.setBaseValue(0, 0.25f);

    matrix.setMonoSourceValue(src.index, 0.6f);
    oracle.setMonoSource(0, 0.6f);

    matrix.process();
    auto [oracle_mono, oracle_poly] = oracle.process();

    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.55f));
    REQUIRE(oracle_mono[0] == Catch::Approx(0.55f));
    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(oracle_mono[0]));
}

TEST_CASE("K1: Oracle verification for poly patch", "[modmatrix][oracle]")
{
    ModMatrix matrix(SmallConfig);
    ModMatrixOracle oracle(4, 8, 16);

    auto& src = matrix.registerSource("src", ModSrcType::Poly, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);
    oracle.addSource(false, false, false);
    oracle.addDestination(false);

    matrix.addConnection(src, dst, 1.0f, false);
    oracle.addConnection(0, 0, 1.0f, false);

    matrix.setBaseValue(dst.index, 0.0f);
    oracle.setBaseValue(0, 0.0f);

    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(2);
    oracle.active_voices = {0, 2};

    matrix.setPolySourceValue(src.index, 0, 0.3f);
    matrix.setPolySourceValue(src.index, 2, 0.7f);
    oracle.setPolySource(0, 0, 0.3f);
    oracle.setPolySource(0, 2, 0.7f);

    matrix.process();
    auto [oracle_mono, oracle_poly] = oracle.process();

    REQUIRE(matrix.getPolyModValue(dst.index, 0) == Catch::Approx(0.3f));
    REQUIRE(matrix.getPolyModValue(dst.index, 2) == Catch::Approx(0.7f));
    REQUIRE(oracle_poly[0][0] == Catch::Approx(0.3f));
    REQUIRE(oracle_poly[2][0] == Catch::Approx(0.7f));
}

TEST_CASE("K1: Oracle verification for bipolar mapping", "[modmatrix][oracle]")
{
    ModMatrix matrix(SmallConfig);
    ModMatrixOracle oracle(4, 8, 16);

    auto& src = matrix.registerSource("src", ModSrcType::Mono, true);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);
    oracle.addSource(true, false, true);
    oracle.addDestination(true);

    matrix.addConnection(src, dst, 1.0f, true);
    oracle.addConnection(0, 0, 1.0f, true);

    matrix.setBaseValue(dst.index, 0.5f);
    oracle.setBaseValue(0, 0.5f);

    matrix.setMonoSourceValue(src.index, 0.0f);
    oracle.setMonoSource(0, 0.0f);

    matrix.process();
    auto [oracle_mono, oracle_poly] = oracle.process();

    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));
    REQUIRE(oracle_mono[0] == Catch::Approx(0.5f));
}

TEST_CASE("K1: Oracle verification for MP connection", "[modmatrix][oracle]")
{
    ModMatrix matrix(SmallConfig);
    ModMatrixOracle oracle(4, 8, 16);

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);
    oracle.addSource(true, false, false);
    oracle.addDestination(false);

    matrix.addConnection(src, dst, 0.5f, false);
    oracle.addConnection(0, 0, 0.5f, false);

    matrix.setBaseValue(dst.index, 0.2f);
    oracle.setBaseValue(0, 0.2f);

    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(1);
    oracle.active_voices = {0, 1};

    matrix.setMonoSourceValue(src.index, 0.4f);
    oracle.setMonoSource(0, 0.4f);

    matrix.process();
    auto [oracle_mono, oracle_poly] = oracle.process();

    const float expected = 0.4f;
    REQUIRE(matrix.getPolyModValue(dst.index, 0) == Catch::Approx(expected));
    REQUIRE(matrix.getPolyModValue(dst.index, 1) == Catch::Approx(expected));
    REQUIRE(oracle_poly[0][0] == Catch::Approx(expected));
    REQUIRE(oracle_poly[1][0] == Catch::Approx(expected));
}

TEST_CASE("A4: Source enumeration and name population", "[modmatrix][registration]")
{
    ModMatrix matrix(SmallConfig);

    auto& src1 = matrix.registerSource("LFO1", ModSrcType::Mono, true);
    auto& src2 = matrix.registerSource("ENV1", ModSrcType::Poly, false);

    REQUIRE(matrix.getSourceCount() == 2);
    REQUIRE(matrix.getSource(0).name == "LFO1");
    REQUIRE(matrix.getSource(1).name == "ENV1");
    REQUIRE(matrix.getSource(0).index == src1.index);
}

TEST_CASE("A5: Destination enumeration and name population", "[modmatrix][registration]")
{
    ModMatrix matrix(SmallConfig);

    auto& dst1 = matrix.registerDestination("Cutoff", ModDstMode::Poly);
    auto& dst2 = matrix.registerDestination("Gain", ModDstMode::Mono);

    REQUIRE(matrix.getDestinationCount() == 2);
    REQUIRE(matrix.getDestination(0).name == "Cutoff");
    REQUIRE(matrix.getDestination(1).name == "Gain");
    REQUIRE(matrix.getDestination(0).index == dst1.index);
}

TEST_CASE("A6: Find source/destination by name", "[modmatrix][registration]")
{
    ModMatrix matrix(SmallConfig);

    matrix.registerSource("LFO1", ModSrcType::Mono);
    matrix.registerDestination("Cutoff", ModDstMode::Poly);

    SECTION("Find existing source") {
        auto* src = matrix.findSource("LFO1");
        REQUIRE(src != nullptr);
        REQUIRE(src->name == "LFO1");
    }

    SECTION("Find non-existent source returns nullptr") {
        REQUIRE(matrix.findSource("NOPE") == nullptr);
    }

    SECTION("Find existing destination") {
        auto* dst = matrix.findDestination("Cutoff");
        REQUIRE(dst != nullptr);
        REQUIRE(dst->name == "Cutoff");
    }

    SECTION("Find non-existent destination returns nullptr") {
        REQUIRE(matrix.findDestination("NOPE") == nullptr);
    }
}

TEST_CASE("F5: Connection enumeration", "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    REQUIRE(matrix.getConnections().empty());

    matrix.addConnection(src, dst, 0.5f);
    REQUIRE(matrix.getConnections().size() == 1);
    REQUIRE(matrix.getConnections()[0].src_idx == src.index);
    REQUIRE(matrix.getConnections()[0].dst_idx == dst.index);
}

TEST_CASE("F6: Remove depth mod connection", "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, dst, 0.5f);
    auto depth_conn = matrix.addDepthModulation(depth_src, conn, 1.0f);

    REQUIRE(matrix.getConnections().size() == 2);

    REQUIRE(matrix.removeConnection(depth_conn));
    REQUIRE(matrix.getConnections().size() == 1);
    REQUIRE(!matrix.getConnections()[0].isDepthMod());
}

TEST_CASE("F7: Removing a param connection cascade-deletes its depth mods", "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, dst, 0.5f);
    matrix.addDepthModulation(depth_src, conn, 1.0f);
    REQUIRE(matrix.getConnections().size() == 2);

    REQUIRE(matrix.removeConnection(conn));
    REQUIRE(matrix.getConnections().empty());
}

TEST_CASE("F8: Slot freed by cascade is cleanly reusable (no stale depth mod attaches)",
          "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& s1 = matrix.registerSource("s1", ModSrcType::Mono);
    auto& s2 = matrix.registerSource("s2", ModSrcType::Mono);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono);
    auto& d1 = matrix.registerDestination("d1", ModDstMode::Mono);
    auto& d2 = matrix.registerDestination("d2", ModDstMode::Mono);

    auto a = matrix.addConnection(s1, d1, 0.5f);
    matrix.addDepthModulation(depth_src, a, 1.0f);
    const uint16_t freed = a.depth_slot;

    REQUIRE(matrix.removeConnection(a));

    // New connection reclaims the freed slot; it must NOT inherit the dead depth mod.
    auto c = matrix.addConnection(s2, d2, 0.5f);
    REQUIRE(c.depth_slot == freed);

    matrix.setBaseValue(d2.index, 0.0f);
    matrix.setMonoSourceValue(s2.index, 1.0f);
    matrix.setMonoSourceValue(depth_src.index, 1.0f);  // would've amplified depth if attached
    matrix.process();

    REQUIRE(matrix.getModValue(d2.index) == Catch::Approx(0.5f));
}

TEST_CASE("F9: reassignSource preserves depth_slot and attached depth mods",
          "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& s1 = matrix.registerSource("s1", ModSrcType::Mono);
    auto& s2 = matrix.registerSource("s2", ModSrcType::Mono);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(s1, dst, 0.5f, false);
    auto dm = matrix.addDepthModulation(depth_src, conn, 0.25f);
    const uint16_t slot_before = conn.depth_slot;
    const uint16_t dm_slot_before = dm.depth_slot;

    auto reassigned = matrix.reassignSource(conn, s2);

    REQUIRE(reassigned.depth_slot == slot_before);
    REQUIRE(reassigned.src_idx == s2.index);
    REQUIRE(reassigned.dst_idx == dst.index);
    REQUIRE(matrix.getConnections().size() == 2);

    // Depth mod still attached to the same slot
    auto found_dm = matrix.findDepthMod(depth_src.index, slot_before);
    REQUIRE(found_dm.has_value());
    REQUIRE(found_dm->depth_slot == dm_slot_before);

    // And processes correctly through the new source
    matrix.setBaseValue(dst.index, 0.0f);
    matrix.setMonoSourceValue(s2.index, 1.0f);
    matrix.setMonoSourceValue(depth_src.index, 0.0f);  // depth mod contributes 0
    matrix.process();
    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));
}

TEST_CASE("F10: reassignSource merges into existing peer", "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& s1 = matrix.registerSource("s1", ModSrcType::Mono);
    auto& s2 = matrix.registerSource("s2", ModSrcType::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto a = matrix.addConnection(s1, dst, 0.25f, false);
    auto b = matrix.addConnection(s2, dst, 0.75f, true);
    const uint16_t b_slot = b.depth_slot;

    // Reassign A's source to s2 -> collides with B. A's (0.25, non-bipolar) values win on B.
    auto merged = matrix.reassignSource(a, s2);

    REQUIRE(matrix.getConnections().size() == 1);
    REQUIRE(merged.depth_slot == b_slot);
    REQUIRE(merged.src_idx == s2.index);
    REQUIRE(merged.getDepth() == Catch::Approx(0.25f));
    REQUIRE(!merged.isBipolar());
}

TEST_CASE("F11: reassignSource merge transfers the moving conn's depth mods onto the peer",
          "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& s1 = matrix.registerSource("s1", ModSrcType::Mono);
    auto& s2 = matrix.registerSource("s2", ModSrcType::Mono);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto a = matrix.addConnection(s1, dst, 0.25f);
    matrix.addDepthModulation(depth_src, a, 0.5f);
    auto b = matrix.addConnection(s2, dst, 0.75f);  // no depth mods on B
    REQUIRE(matrix.getConnections().size() == 3);

    matrix.reassignSource(a, s2);  // merge into B; A's depth mod rehomes onto B

    REQUIRE(matrix.getConnections().size() == 2);
    auto transferred = matrix.findDepthMod(depth_src.index, b.depth_slot);
    REQUIRE(transferred.has_value());
    REQUIRE(transferred->getDepth() == Catch::Approx(0.5f));
}

TEST_CASE("F11b: reassignSource merge keeps peer's existing depth mod on conflict (peer wins)",
          "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& s1 = matrix.registerSource("s1", ModSrcType::Mono);
    auto& s2 = matrix.registerSource("s2", ModSrcType::Mono);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto a = matrix.addConnection(s1, dst, 0.25f);
    matrix.addDepthModulation(depth_src, a, 0.3f);  // A's depth mod
    auto b = matrix.addConnection(s2, dst, 0.75f);
    matrix.addDepthModulation(depth_src, b, 0.8f);  // B's depth mod from same source
    REQUIRE(matrix.getConnections().size() == 4);

    matrix.reassignSource(a, s2);

    // Only B + B's depth mod survive; A and A's depth mod are gone.
    REQUIRE(matrix.getConnections().size() == 2);
    auto kept = matrix.findDepthMod(depth_src.index, b.depth_slot);
    REQUIRE(kept.has_value());
    REQUIRE(kept->getDepth() == Catch::Approx(0.8f));  // B's value wins
}

TEST_CASE("F12: reassignSource is a no-op when newSrc equals current src",
          "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& s1 = matrix.registerSource("s1", ModSrcType::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(s1, dst, 0.5f);
    auto result = matrix.reassignSource(conn, s1);

    REQUIRE(result.depth_slot == conn.depth_slot);
    REQUIRE(matrix.getConnections().size() == 1);
}

TEST_CASE("F13: reassignSource on a depth mod keeps it attached to the same target slot",
          "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono);
    auto& d1 = matrix.registerSource("d1", ModSrcType::Mono);
    auto& d2 = matrix.registerSource("d2", ModSrcType::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, dst, 0.0f);
    auto dm = matrix.addDepthModulation(d1, conn, 0.5f);
    const uint16_t dm_slot = dm.depth_slot;
    const uint16_t target_slot = conn.depth_slot;

    auto reassigned = matrix.reassignSource(dm, d2);

    REQUIRE(reassigned.isDepthMod());
    REQUIRE(reassigned.src_idx == d2.index);
    REQUIRE(reassigned.dst_idx == target_slot);
    REQUIRE(reassigned.depth_slot == dm_slot);
}

TEST_CASE("F13b: reassignSource merge on a depth mod overwrites peer depth and flags",
          "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono);
    auto& d1 = matrix.registerSource("d1", ModSrcType::Mono);
    auto& d2 = matrix.registerSource("d2", ModSrcType::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, dst, 0.5f);
    auto a = matrix.addDepthModulation(d1, conn, 0.25f, false);
    auto b = matrix.addDepthModulation(d2, conn, 0.75f, true);
    const uint16_t target_slot = conn.depth_slot;
    const uint16_t b_slot = b.depth_slot;

    auto merged = matrix.reassignSource(a, d2);

    REQUIRE(matrix.getConnections().size() == 2);
    REQUIRE(merged.isDepthMod());
    REQUIRE(merged.depth_slot == b_slot);
    REQUIRE(merged.src_idx == d2.index);
    REQUIRE(merged.dst_idx == target_slot);
    REQUIRE(merged.getDepth() == Catch::Approx(0.25f));
    REQUIRE(!merged.isBipolar());

    auto kept = matrix.findDepthMod(d2.index, target_slot);
    REQUIRE(kept.has_value());
    REQUIRE(kept->depth_slot == b_slot);
    REQUIRE(!kept->isBipolar());
    REQUIRE(!matrix.findDepthMod(d1.index, target_slot).has_value());

    matrix.setBaseValue(dst.index, 0.0f);
    matrix.setMonoSourceValue(main_src.index, 1.0f);
    matrix.setMonoSourceValue(d2.index, 0.0f);  // would subtract if B's old bipolar flag survived
    matrix.process();

    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));
}

TEST_CASE("F14: reassignDestination preserves depth_slot and attached depth mods",
          "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono);
    auto& d1 = matrix.registerDestination("d1", ModDstMode::Mono);
    auto& d2 = matrix.registerDestination("d2", ModDstMode::Mono);

    auto conn = matrix.addConnection(src, d1, 0.5f);
    matrix.addDepthModulation(depth_src, conn, 0.25f);
    const uint16_t slot_before = conn.depth_slot;

    auto reassigned = matrix.reassignDestination(conn, d2);

    REQUIRE(reassigned.depth_slot == slot_before);
    REQUIRE(reassigned.dst_idx == d2.index);
    REQUIRE(matrix.findDepthMod(depth_src.index, slot_before).has_value());

    matrix.setBaseValue(d1.index, 0.0f);
    matrix.setBaseValue(d2.index, 0.0f);
    matrix.setMonoSourceValue(src.index, 1.0f);
    matrix.setMonoSourceValue(depth_src.index, 0.0f);
    matrix.process();

    REQUIRE(matrix.getModValue(d1.index) == Catch::Approx(0.0f));
    REQUIRE(matrix.getModValue(d2.index) == Catch::Approx(0.5f));
}

TEST_CASE("F15: reassignDestination across mono/poly re-partitions into correct program bucket",
          "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono);
    auto& mono_dst = matrix.registerDestination("mono_dst", ModDstMode::Mono);
    auto& poly_dst = matrix.registerDestination("poly_dst", ModDstMode::Poly);

    auto conn = matrix.addConnection(src, mono_dst, 0.5f);
    matrix.reassignDestination(conn, poly_dst);

    matrix.notifyVoiceOn(0);
    matrix.setBaseValue(poly_dst.index, 0.0f);
    matrix.setMonoSourceValue(src.index, 1.0f);
    matrix.process();

    REQUIRE(matrix.getPolyModValue(poly_dst.index, 0) == Catch::Approx(0.5f));
    REQUIRE(matrix.getModValue(mono_dst.index) == Catch::Approx(0.0f));
}

TEST_CASE("F15b: reassignSource across mono/poly-effective sources re-partitions into correct program bucket",
          "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& mono_src = matrix.registerSource("mono_src", ModSrcType::Both, false, ModSrcMode::Mono);
    auto& poly_src = matrix.registerSource("poly_src", ModSrcType::Both, false, ModSrcMode::Poly);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);

    auto conn = matrix.addConnection(mono_src, dst, 1.0f);
    const uint16_t slot = conn.depth_slot;
    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(1);
    matrix.setBaseValue(dst.index, 0.0f);

    matrix.setMonoSourceValue(mono_src.index, 0.25f);
    matrix.process();
    REQUIRE(matrix.getPolyModValue(dst.index, 0) == Catch::Approx(0.25f));
    REQUIRE(matrix.getPolyModValue(dst.index, 1) == Catch::Approx(0.25f));

    auto poly_conn = matrix.reassignSource(conn, poly_src);
    REQUIRE(poly_conn.depth_slot == slot);

    matrix.setPolySourceValue(poly_src.index, 0, 0.1f);
    matrix.setPolySourceValue(poly_src.index, 1, 0.8f);
    matrix.process();
    REQUIRE(matrix.getPolyModValue(dst.index, 0) == Catch::Approx(0.1f));
    REQUIRE(matrix.getPolyModValue(dst.index, 1) == Catch::Approx(0.8f));

    auto mono_conn = matrix.reassignSource(poly_conn, mono_src);
    REQUIRE(mono_conn.depth_slot == slot);

    matrix.setMonoSourceValue(mono_src.index, 0.6f);
    matrix.process();
    REQUIRE(matrix.getPolyModValue(dst.index, 0) == Catch::Approx(0.6f));
    REQUIRE(matrix.getPolyModValue(dst.index, 1) == Catch::Approx(0.6f));
}

TEST_CASE("F16: reassignDestination merges into existing peer", "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono);
    auto& d1 = matrix.registerDestination("d1", ModDstMode::Mono);
    auto& d2 = matrix.registerDestination("d2", ModDstMode::Mono);

    auto a = matrix.addConnection(src, d1, 0.25f);
    auto b = matrix.addConnection(src, d2, 0.75f);
    const uint16_t b_slot = b.depth_slot;

    auto merged = matrix.reassignDestination(a, d2);

    REQUIRE(matrix.getConnections().size() == 1);
    REQUIRE(merged.depth_slot == b_slot);
    REQUIRE(merged.dst_idx == d2.index);
    REQUIRE(merged.getDepth() == Catch::Approx(0.25f));
}

TEST_CASE("F16b: reassignDestination merge transfers only non-conflicting depth mods onto the peer",
          "[modmatrix][connections][reassign]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono);
    auto& dm1 = matrix.registerSource("dm1", ModSrcType::Mono);
    auto& dm2 = matrix.registerSource("dm2", ModSrcType::Mono);
    auto& d1 = matrix.registerDestination("d1", ModDstMode::Mono);
    auto& d2 = matrix.registerDestination("d2", ModDstMode::Mono);

    auto a = matrix.addConnection(main_src, d1, 0.1f);
    const uint16_t a_slot = a.depth_slot;
    matrix.addDepthModulation(dm1, a, 0.2f);
    matrix.addDepthModulation(dm2, a, 0.4f);

    auto b = matrix.addConnection(main_src, d2, 0.7f);
    const uint16_t b_slot = b.depth_slot;
    matrix.addDepthModulation(dm2, b, 0.3f);
    REQUIRE(matrix.getConnections().size() == 5);

    auto merged = matrix.reassignDestination(a, d2);

    REQUIRE(matrix.getConnections().size() == 3);
    REQUIRE(merged.depth_slot == b_slot);
    REQUIRE(merged.getDepth() == Catch::Approx(0.1f));

    auto transferred = matrix.findDepthMod(dm1.index, b_slot);
    REQUIRE(transferred.has_value());
    REQUIRE(transferred->getDepth() == Catch::Approx(0.2f));

    auto kept = matrix.findDepthMod(dm2.index, b_slot);
    REQUIRE(kept.has_value());
    REQUIRE(kept->getDepth() == Catch::Approx(0.3f));

    REQUIRE(!matrix.findDepthMod(dm1.index, a_slot).has_value());
    REQUIRE(!matrix.findDepthMod(dm2.index, a_slot).has_value());

    matrix.setBaseValue(d1.index, 0.0f);
    matrix.setBaseValue(d2.index, 0.0f);
    matrix.setMonoSourceValue(main_src.index, 1.0f);
    matrix.setMonoSourceValue(dm1.index, 1.0f);
    matrix.setMonoSourceValue(dm2.index, 1.0f);
    matrix.process();

    REQUIRE(matrix.getModValue(d1.index) == Catch::Approx(0.0f));
    REQUIRE(matrix.getModValue(d2.index) == Catch::Approx(0.6f));
}

TEST_CASE("M1: Empty destination has zero offset range", "[modmatrix][offset_range]")
{
    ModMatrix matrix(SmallConfig);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
    REQUIRE(min_off == Catch::Approx(0.0f));
    REQUIRE(max_off == Catch::Approx(0.0f));
}

TEST_CASE("M2: Unipolar mapping contributes [min(0,d), max(0,d)]", "[modmatrix][offset_range]")
{
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    SECTION("Positive depth") {
        matrix.addConnection(src, dst, 0.5f, false);
        auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
        REQUIRE(min_off == Catch::Approx(0.0f));
        REQUIRE(max_off == Catch::Approx(0.5f));
    }

    SECTION("Negative depth") {
        matrix.addConnection(src, dst, -0.3f, false);
        auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
        REQUIRE(min_off == Catch::Approx(-0.3f));
        REQUIRE(max_off == Catch::Approx(0.0f));
    }

    SECTION("Zero depth") {
        matrix.addConnection(src, dst, 0.0f, false);
        auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
        REQUIRE(min_off == Catch::Approx(0.0f));
        REQUIRE(max_off == Catch::Approx(0.0f));
    }
}

TEST_CASE("M3: Bipolar mapping contributes [-|d|/2, +|d|/2] regardless of sign",
          "[modmatrix][offset_range]")
{
    // Peak-to-peak of a bipolar-mapped connection equals |depth|, so the range splits into ±|d|/2.
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, true);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    SECTION("Positive depth") {
        matrix.addConnection(src, dst, 0.4f, true);
        auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
        REQUIRE(min_off == Catch::Approx(-0.2f));
        REQUIRE(max_off == Catch::Approx(+0.2f));
    }

    SECTION("Negative depth gives symmetric range") {
        matrix.addConnection(src, dst, -0.4f, true);
        auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
        REQUIRE(min_off == Catch::Approx(-0.2f));
        REQUIRE(max_off == Catch::Approx(+0.2f));
    }
}

TEST_CASE("M4: Multiple connections sum independently", "[modmatrix][offset_range]")
{
    ModMatrix matrix(SmallConfig);
    auto& s_bi = matrix.registerSource("s_bi", ModSrcType::Mono, true);
    auto& s_up_pos = matrix.registerSource("s_up_pos", ModSrcType::Mono, false);
    auto& s_up_neg = matrix.registerSource("s_up_neg", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    matrix.addConnection(s_bi, dst, 0.2f, true);       // -> [-0.1, +0.1]
    matrix.addConnection(s_up_pos, dst, 0.3f, false);  // -> [ 0.0, +0.3]
    matrix.addConnection(s_up_neg, dst, -0.1f, false); // -> [-0.1,  0.0]

    auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
    REQUIRE(min_off == Catch::Approx(-0.2f));
    REQUIRE(max_off == Catch::Approx(+0.4f));
}

TEST_CASE("M5: Source bipolar flag does not affect range; only connection mapping does",
          "[modmatrix][offset_range]")
{
    ModMatrix matrix(SmallConfig);
    auto& bipolar_src = matrix.registerSource("bi", ModSrcType::Mono, true);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    // Bipolar source, but connection uses *unipolar* mapping -> range should be [0, d], not [-d, d].
    matrix.addConnection(bipolar_src, dst, 0.5f, false);

    auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
    REQUIRE(min_off == Catch::Approx(0.0f));
    REQUIRE(max_off == Catch::Approx(0.5f));
}

TEST_CASE("M6: Depth-mod connections are excluded", "[modmatrix][offset_range]")
{
    ModMatrix matrix(SmallConfig);
    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, dst, 0.4f, false);  // [0, 0.4]

    auto before = matrix.getModOffsetRange(dst.index);

    matrix.addDepthModulation(depth_src, conn, 0.9f);

    auto after = matrix.getModOffsetRange(dst.index);
    REQUIRE(after.first == Catch::Approx(before.first));
    REQUIRE(after.second == Catch::Approx(before.second));
    REQUIRE(after.first == Catch::Approx(0.0f));
    REQUIRE(after.second == Catch::Approx(0.4f));
}

TEST_CASE("M7: Only connections targeting the queried destination are counted",
          "[modmatrix][offset_range]")
{
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst_a = matrix.registerDestination("dst_a", ModDstMode::Mono);
    auto& dst_b = matrix.registerDestination("dst_b", ModDstMode::Mono);

    matrix.addConnection(src, dst_a, 0.5f, false);
    matrix.addConnection(src, dst_b, 0.2f, false);

    auto range_a = matrix.getModOffsetRange(dst_a.index);
    auto range_b = matrix.getModOffsetRange(dst_b.index);

    REQUIRE(range_a.second == Catch::Approx(0.5f));
    REQUIRE(range_b.second == Catch::Approx(0.2f));
}

TEST_CASE("M8: Reported range bounds process() output at source extremes",
          "[modmatrix][offset_range]")
{
    // Pin sources to their extremes and confirm process() output - base stays within the
    // reported [min, max]. Guards against drift in process()'s per-connection math.
    ModMatrix matrix(SmallConfig);
    auto& s_bi = matrix.registerSource("s_bi", ModSrcType::Mono, true);
    auto& s_up = matrix.registerSource("s_up", ModSrcType::Mono, false);

    // Identity scale so plain == normalized.
    applause::ValueScaleInfo identity{0.0f, 1.0f, applause::ValueScaling::linear()};
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);

    matrix.addConnection(s_bi, dst, 0.3f, true);
    matrix.addConnection(s_up, dst, 0.2f, false);

    const float base = 0.5f;
    matrix.setBaseValue(dst.index, base);

    auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
    REQUIRE(min_off == Catch::Approx(-0.15f));
    REQUIRE(max_off == Catch::Approx(+0.35f));

    matrix.setMonoSourceValue(s_bi.index, +1.0f);
    matrix.setMonoSourceValue(s_up.index, +1.0f);
    matrix.process();
    REQUIRE(matrix.getModValue(dst.index) - base <= max_off + 1e-5f);

    matrix.setMonoSourceValue(s_bi.index, -1.0f);
    matrix.setMonoSourceValue(s_up.index, 0.0f);
    matrix.process();
    REQUIRE(matrix.getModValue(dst.index) - base >= min_off - 1e-5f);
}

// addConnection without an explicit bipolar_mapping picks a default:
//   bipolar source           → bipolar-mapped
//   unipolar src, base 1/3..2/3 → bipolar-mapped
//   unipolar src, base <1/3  → unipolar-mapped, depth forced positive
//   unipolar src, base >2/3  → unipolar-mapped, depth forced negative
// An explicit bipolar_mapping argument always wins.

TEST_CASE("N1: Bipolar source defaults to bipolar-mapped regardless of base",
          "[modmatrix][connections][default_mapping]")
{
    auto check = [](float base) {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, true);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);
        matrix.setBaseValue(dst.index, base);
        auto conn = matrix.addConnection(src, dst, 0.5f);
        REQUIRE(conn.isBipolar());
        REQUIRE(conn.getDepth() == Catch::Approx(0.5f));
    };

    SECTION("base = 0.0") { check(0.0f); }
    SECTION("base = 0.5") { check(0.5f); }
    SECTION("base = 1.0") { check(1.0f); }
}

TEST_CASE("N2: Unipolar source, base near min → unipolar-mapped, depth forced positive",
          "[modmatrix][connections][default_mapping]")
{
    applause::ValueScaleInfo identity{0.0f, 1.0f, applause::ValueScaling::linear()};
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);
    matrix.setBaseValue(dst.index, 0.1f);

    auto conn = matrix.addConnection(src, dst, -0.8f);
    REQUIRE(!conn.isBipolar());
    REQUIRE(conn.getDepth() == Catch::Approx(+0.8f));
}

TEST_CASE("N3: Unipolar source, base near center → bipolar-mapped",
          "[modmatrix][connections][default_mapping]")
{
    applause::ValueScaleInfo identity{0.0f, 1.0f, applause::ValueScaling::linear()};
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);
    matrix.setBaseValue(dst.index, 0.5f);

    auto conn = matrix.addConnection(src, dst, 0.6f);
    REQUIRE(conn.isBipolar());
    REQUIRE(conn.getDepth() == Catch::Approx(0.6f));
}

TEST_CASE("N4: Unipolar source, base near max → unipolar-mapped, depth forced negative",
          "[modmatrix][connections][default_mapping]")
{
    applause::ValueScaleInfo identity{0.0f, 1.0f, applause::ValueScaling::linear()};
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);
    matrix.setBaseValue(dst.index, 0.9f);

    auto conn = matrix.addConnection(src, dst, 0.8f);
    REQUIRE(!conn.isBipolar());
    REQUIRE(conn.getDepth() == Catch::Approx(-0.8f));
}

TEST_CASE("N5: Explicit bipolar_mapping overrides the smart default",
          "[modmatrix][connections][default_mapping]")
{
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);
    matrix.setBaseValue(dst.index, 0.0f);

    auto conn = matrix.addConnection(src, dst, 0.5f, true);
    REQUIRE(conn.isBipolar());
    REQUIRE(conn.getDepth() == Catch::Approx(0.5f));
}

TEST_CASE("N6: Smart default integrates with getModOffsetRange",
          "[modmatrix][connections][default_mapping][offset_range]")
{
    applause::ValueScaleInfo identity{0.0f, 1.0f, applause::ValueScaling::linear()};

    SECTION("Bipolar source centered on 0.5 → reported range [-d/2, +d/2]") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, true);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);
        matrix.setBaseValue(dst.index, 0.5f);
        matrix.addConnection(src, dst, 1.0f);

        auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
        REQUIRE(min_off == Catch::Approx(-0.5f));
        REQUIRE(max_off == Catch::Approx(+0.5f));
    }

    SECTION("Unipolar source at base 0.0 → reported range [0, +d]") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);
        matrix.setBaseValue(dst.index, 0.0f);
        matrix.addConnection(src, dst, 1.0f);

        auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
        REQUIRE(min_off == Catch::Approx(0.0f));
        REQUIRE(max_off == Catch::Approx(+1.0f));
    }

    SECTION("Unipolar source at base 1.0 → reported range [-d, 0]") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);
        matrix.setBaseValue(dst.index, 1.0f);
        matrix.addConnection(src, dst, 1.0f);

        auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
        REQUIRE(min_off == Catch::Approx(-1.0f));
        REQUIRE(max_off == Catch::Approx(0.0f));
    }
}
