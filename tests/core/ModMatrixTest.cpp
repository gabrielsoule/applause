/**
 * Unit tests for ModMatrix.h.
 *
 * Author's note:
 * Generated mostly by Claude Code and GPT-5.2 Pro in collaboration.
 * They make a good duo. LLMs are pretty good at tests.
 *
 * Covers specification sections A-L:
 * A: Registration tests
 * B: Voice management tests
 * C: Base value and scaling tests
 * D: Source value tests
 * E: Mapping semantics tests
 * F: Connection lifecycle tests
 * G: Mode toggle tests
 * H: Reset and determinism tests
 * I: ModParamHandle tests
 * J: NYI behavior tests
 * K: Oracle/property-based tests
 * L: Performance tests (optional)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <applause/core/ModMatrix.h>
#include <applause/extensions/ParamsExtension.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace applause;

// Config constants for different test scenarios
constexpr ModMatrix::Config SmallConfig{4, 8, 16, 32};       // Simple tests
constexpr ModMatrix::Config StandardConfig{16, 32, 64, 128}; // Realistic synth
constexpr ModMatrix::Config MinimalConfig{1, 1, 1, 1};       // Edge cases

/**
 * A simple, naive reference implementation of the modulation matrix.
 * Designed for correctness verification, not performance.
 */
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

    // Unified connection structure matching the new ModMatrix design
    struct Connection {
        uint16_t src_idx;
        uint16_t target;       // dst_idx for param connections, target_slot for depth mods
        uint16_t depth_slot;   // Where THIS connection's depth is stored
        bool is_depth_mod;
        bool bipolar_mapping;
    };

    std::vector<Source> sources;
    std::vector<Destination> destinations;
    std::vector<Connection> connections;  // Unified: both param and depth mod connections
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
        // Depth mod connections now also allocate a depth slot
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

    /**
     * Implements the exact bipolar normalization logic from ModMatrix::process().
     * Under the Serum-style peak-to-peak convention, a bipolar-mapped connection
     * contributes offsets in [-0.5, +0.5] so that `depth` equals peak-to-peak swing.
     */
    static float applyBipolarNormalization(float src_val, bool src_bipolar, bool bipolar_mapping) {
        if (src_bipolar) {
            src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
        }
        if (bipolar_mapping) {
            src_val -= 0.5f;                    // [0,1]   -> [-0.5,+0.5]
        }
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

        // Calculate effective depths
        std::vector<float> mono_depth = depth_base;
        std::vector<std::vector<float>> poly_depth(num_voices);
        for (auto& pd : poly_depth) {
            pd = depth_base;
        }

        // Mono depth modulation (process depth mod connections with mono sources)
        for (const auto& conn : connections) {
            if (!conn.is_depth_mod) continue;
            if (effectivelyMono(conn.src_idx)) {
                float src_val = mono_src_values[conn.src_idx];
                src_val = applyBipolarNormalization(src_val, sources[conn.src_idx].bipolar, conn.bipolar_mapping);
                // conn.target = slot we're modulating, conn.depth_slot = where this conn's depth is stored
                float depth = depth_base[conn.depth_slot];
                mono_depth[conn.target] += src_val * depth;
            }
        }

        // Copy mono depth to poly, apply poly depth modulation
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

        // Apply parameter connections
        for (const auto& conn : connections) {
            if (conn.is_depth_mod) continue;

            bool src_mono = effectivelyMono(conn.src_idx);
            bool dst_mono = destinations[conn.target].is_mono;
            bool src_bipolar = sources[conn.src_idx].bipolar;

            if (src_mono && dst_mono) {
                // MM
                float src_val = mono_src_values[conn.src_idx];
                src_val = applyBipolarNormalization(src_val, src_bipolar, conn.bipolar_mapping);
                mono_out[conn.target] += src_val * mono_depth[conn.depth_slot];
            }
            else if (src_mono && !dst_mono) {
                // MP
                for (uint16_t v : active_voices) {
                    float src_val = mono_src_values[conn.src_idx];
                    src_val = applyBipolarNormalization(src_val, src_bipolar, conn.bipolar_mapping);
                    poly_out[v][conn.target] += src_val * poly_depth[v][conn.depth_slot];
                }
            }
            else if (!src_mono && !dst_mono) {
                // PP
                for (uint16_t v : active_voices) {
                    float src_val = poly_src_values[v][conn.src_idx];
                    src_val = applyBipolarNormalization(src_val, src_bipolar, conn.bipolar_mapping);
                    poly_out[v][conn.target] += src_val * poly_depth[v][conn.depth_slot];
                }
            }
            // PM: NYI
        }

        // Apply scaling
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
        REQUIRE(lfo1.mode == ModSrcMode::Mono);  // defaultMode was Mono
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
        // Create a connection and verify poly processing works
        auto& src = matrix.registerSource("src", ModSrcType::Mono);
        matrix.addConnection(src, cutoff, 0.5f);
        matrix.setBaseValue(cutoff.index, 1000.0f);  // plain value
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.notifyVoiceOn(0);
        matrix.process();

        // Poly destination should be modulated
        float result = matrix.getPolyModValue(cutoff.index, 0);
        REQUIRE(result > 1000.0f);  // Modulation increased the value
    }
}

TEST_CASE("B1: Voice on adds voice once (no duplicates)", "[modmatrix][voice]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Poly);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);
    matrix.addConnection(src, dst, 1.0f, false);
    matrix.setBaseValue(dst.index, 0.0f);

    // Call notifyVoiceOn multiple times for same voice
    matrix.notifyVoiceOn(2);
    matrix.notifyVoiceOn(2);
    matrix.notifyVoiceOn(2);

    // Set source value
    matrix.setPolySourceValue(src.index, 2, 0.5f);
    matrix.process();

    // If voice were added multiple times, output would be wrong
    // With single addition, output should be base + src * depth = 0 + 0.5 * 1.0 = 0.5
    REQUIRE(matrix.getPolyModValue(dst.index, 2) == Catch::Approx(0.5f));
}

TEST_CASE("B2: Voice off removes voice from processing", "[modmatrix][voice]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Poly);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);
    matrix.addConnection(src, dst, 1.0f, false);

    // Activate voice 1, then deactivate
    matrix.notifyVoiceOn(1);
    matrix.setBaseValue(dst.index, 0.5f);
    matrix.setPolySourceValue(src.index, 1, 0.3f);
    matrix.process();

    float before_off = matrix.getPolyModValue(dst.index, 1);
    REQUIRE(before_off == Catch::Approx(0.8f));  // 0.5 + 0.3 = 0.8

    // Now deactivate voice and change source
    matrix.notifyVoiceOff(1);
    matrix.setPolySourceValue(src.index, 1, 1.0f);  // Change source
    matrix.process();

    // Voice 1 should not be processed, so it retains old value
    // (poly_dst_buf_ is not reset for inactive voices)
    float after_off = matrix.getPolyModValue(dst.index, 1);
    // The value depends on whether inactive voices are reset or not
    // Current impl: inactive voices are not touched, so they keep their scaled value
    REQUIRE(after_off == Catch::Approx(before_off));
}

TEST_CASE("B3: notifyVoiceOff on inactive voice is safe", "[modmatrix][voice]")
{
    ModMatrix matrix(SmallConfig);

    // Should not crash
    matrix.notifyVoiceOff(0);
    matrix.notifyVoiceOff(1);
    matrix.notifyVoiceOff(3);

    // No assertion/crash means test passes
    REQUIRE(true);
}

TEST_CASE("C1: With no connections, output equals base plain value after scaling", "[modmatrix][scaling]")
{
    ModMatrix matrix(SmallConfig);

    SECTION("Mono destination") {
        applause::ValueScaleInfo scale{0.0f, 100.0f, applause::ValueScaling::linear()};
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, scale);

        matrix.setBaseValue(dst.index, 25.0f);  // plain value
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
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);  // default 0..1 linear
        matrix.setBaseValue(dst.index, 0.5f);
        matrix.process();

        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));
    }
}

TEST_CASE("C2: Clamping behavior - normalized outside [0,1] clamps to min/max", "[modmatrix][scaling]")
{
    ModMatrix matrix(SmallConfig);

    applause::ValueScaleInfo scale{0.0f, 100.0f, applause::ValueScaling::linear()};
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);  // unipolar
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, scale);

    SECTION("Modulation pushing below 0 clamps to min") {
        matrix.setBaseValue(dst.index, 10.0f);  // normalized = 0.1
        matrix.addConnection(src, dst, -0.5f, false);  // negative depth, unipolar mapping
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();

        // normalized = 0.1 + 1.0 * (-0.5) = -0.4 -> clamped to 0.0 -> plain = 0.0
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));
    }

    SECTION("Modulation pushing above 1 clamps to max") {
        matrix.setBaseValue(dst.index, 90.0f);  // normalized = 0.9
        matrix.addConnection(src, dst, 0.5f, false);
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();

        // normalized = 0.9 + 1.0 * 0.5 = 1.4 -> clamped to 1.0 -> plain = 100.0
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(100.0f));
    }
}

TEST_CASE("C4: loadParamBaseValues with extra destinations", "[modmatrix][scaling]")
{
    // TODO: ModMatrix.h loadParamBaseValues() loops to dst_count_ which may exceed
    // param count if extra destinations are registered after registerFromParamsExtension().
    // This test documents current behavior. Fix: store num_param_dests_ and loop only that many.

    ModMatrix matrix(SmallConfig);

    // Create minimal ParamsExtension with 2 params
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

    // Register destinations from params (2 destinations)
    matrix.registerFromParamsExtension(params);

    // Register an extra destination (now we have 3 destinations but only 2 params)
    matrix.registerDestination("extra", ModDstMode::Mono);

    // This should not crash - loadParamBaseValues reads params.getValuesArray()[i]
    // for i < dst_count_, but params only has 2 values
    // Current behavior: may read garbage/out of bounds for index 2
    // For now, just verify no crash with the params we have
    matrix.loadParamBaseValues(params);
    matrix.process();

    // First two destinations should have valid values
    REQUIRE(matrix.getModValue(0) == Catch::Approx(0.5f));
    REQUIRE(matrix.getModValue(1) == Catch::Approx(50.0f));
}

TEST_CASE("D1: Mono source values propagate through MM connections", "[modmatrix][sources]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("MACRO1", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    matrix.addConnection(src, dst, 0.5f, false);  // unipolar mapping
    matrix.setBaseValue(dst.index, 0.25f);

    SECTION("Source value 0.0") {
        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        // result = 0.25 + 0.0 * 0.5 = 0.25
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.25f));
    }

    SECTION("Source value 0.5") {
        matrix.setMonoSourceValue(src.index, 0.5f);
        matrix.process();
        // result = 0.25 + 0.5 * 0.5 = 0.5
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));
    }

    SECTION("Source value 1.0") {
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        // result = 0.25 + 1.0 * 0.5 = 0.75
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

    // Set poly source values for all voices
    matrix.setPolySourceValue(src.index, 0, 0.0f);
    matrix.setPolySourceValue(src.index, 1, 0.5f);
    matrix.setPolySourceValue(src.index, 2, 1.0f);
    matrix.setPolySourceValue(src.index, 3, 0.25f);

    // Activate only voices 0 and 2
    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(2);

    matrix.process();

    SECTION("Active voice 0 reflects its source value") {
        REQUIRE(matrix.getPolyModValue(dst.index, 0) == Catch::Approx(0.0f));
    }

    SECTION("Active voice 2 reflects its source value") {
        REQUIRE(matrix.getPolyModValue(dst.index, 2) == Catch::Approx(1.0f));
    }

    // Note: Inactive voices 1 and 3 are not processed, so their values
    // are whatever was in the buffer (uninitialized or stale)
}

TEST_CASE("E1: Four mapping combinations for main connections", "[modmatrix][mapping]")
{
    // Use identity scaling so plain == normalized
    applause::ValueScaleInfo identity_scale{0.0f, 1.0f, applause::ValueScaling::linear()};

    SECTION("src_bipolar=true, bipolar_mapping=true (peak-to-peak = d, centered)") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, true);  // bipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);
        matrix.addConnection(src, dst, 1.0f, true);  // bipolar mapping
        matrix.setBaseValue(dst.index, 0.5f);

        // Input -1.0: [-1,+1] -> [0,1] = 0.0 -> [-0.5,+0.5] = -0.5
        // result = 0.5 + (-0.5) * 1.0 = 0.0
        matrix.setMonoSourceValue(src.index, -1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        // Input 0.0: [-1,+1] -> [0,1] = 0.5 -> [-0.5,+0.5] = 0.0
        // result = 0.5 + 0.0 * 1.0 = 0.5
        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        // Input +1.0: [-1,+1] -> [0,1] = 1.0 -> [-0.5,+0.5] = +0.5
        // result = 0.5 + 0.5 * 1.0 = 1.0
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("src_bipolar=true, bipolar_mapping=false (half-wave rectify)") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, true);  // bipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);
        matrix.addConnection(src, dst, 1.0f, false);  // unipolar mapping
        matrix.setBaseValue(dst.index, 0.0f);

        // Input -1.0: [-1,+1] -> [0,1] = 0.0 (stays 0.0, no bipolar_mapping)
        // result = 0.0 + 0.0 * 1.0 = 0.0
        matrix.setMonoSourceValue(src.index, -1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        // Input 0.0: [-1,+1] -> [0,1] = 0.5
        // result = 0.0 + 0.5 * 1.0 = 0.5
        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        // Input +1.0: [-1,+1] -> [0,1] = 1.0
        // result = 0.0 + 1.0 * 1.0 = 1.0
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("src_bipolar=false, bipolar_mapping=true (center unipolar on midpoint)") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, false);  // unipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);
        matrix.addConnection(src, dst, 1.0f, true);  // bipolar mapping
        matrix.setBaseValue(dst.index, 0.5f);

        // Input 0.0: no normalization (already [0,1]) -> [-0.5,+0.5] = -0.5
        // result = 0.5 + (-0.5) * 1.0 = 0.0
        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        // Input 0.5: -> [-0.5,+0.5] = 0.0
        // result = 0.5 + 0.0 * 1.0 = 0.5
        matrix.setMonoSourceValue(src.index, 0.5f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        // Input 1.0: -> [-0.5,+0.5] = +0.5
        // result = 0.5 + 0.5 * 1.0 = 1.0
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("src_bipolar=false, bipolar_mapping=false (identity)") {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, false);  // unipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);
        matrix.addConnection(src, dst, 1.0f, false);  // unipolar mapping
        matrix.setBaseValue(dst.index, 0.0f);

        // Input 0.0: identity
        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        // Input 0.5: identity
        matrix.setMonoSourceValue(src.index, 0.5f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        // Input 1.0: identity
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }
}

TEST_CASE("E2: Four mapping combinations for depth modulation", "[modmatrix][mapping]")
{
    applause::ValueScaleInfo identity_scale{0.0f, 1.0f, applause::ValueScaling::linear()};

    SECTION("src_bipolar=true, bipolar_mapping=true for depth mod") {
        ModMatrix matrix(SmallConfig);
        auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
        auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, true);  // bipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);

        // Main connection is unipolar-mapped so its contribution = main_src * effective_depth.
        auto conn = matrix.addConnection(main_src, dst, 0.0f, false);  // base depth = 0
        matrix.addDepthModulation(depth_src, conn, 1.0f, true);  // bipolar mapping
        matrix.setBaseValue(dst.index, 0.5f);
        matrix.setMonoSourceValue(main_src.index, 1.0f);

        // depth_src = -1.0: normalized to 0.0, bipolar_mapping to -0.5
        // effective_depth = 0.0 + (-0.5) * 1.0 = -0.5
        // result = 0.5 + 1.0 * (-0.5) = 0.0
        matrix.setMonoSourceValue(depth_src.index, -1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        // depth_src = 0.0: normalized to 0.5, bipolar_mapping to 0.0
        // effective_depth = 0.0 + 0.0 * 1.0 = 0.0
        // result = 0.5 + 1.0 * 0.0 = 0.5
        matrix.setMonoSourceValue(depth_src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        // depth_src = +1.0: normalized to 1.0, bipolar_mapping to +0.5
        // effective_depth = 0.0 + 0.5 * 1.0 = 0.5
        // result = 0.5 + 1.0 * 0.5 = 1.0
        matrix.setMonoSourceValue(depth_src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("src_bipolar=false, bipolar_mapping=false for depth mod (identity)") {
        ModMatrix matrix(SmallConfig);
        auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
        auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);  // unipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);

        auto conn = matrix.addConnection(main_src, dst, 0.0f, false);
        matrix.addDepthModulation(depth_src, conn, 1.0f, false);  // unipolar mapping
        matrix.setBaseValue(dst.index, 0.0f);
        matrix.setMonoSourceValue(main_src.index, 1.0f);

        // depth_src = 0.0: identity
        // effective_depth = 0.0 + 0.0 * 1.0 = 0.0
        matrix.setMonoSourceValue(depth_src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        // depth_src = 0.5: identity
        // effective_depth = 0.0 + 0.5 * 1.0 = 0.5
        // result = 0.0 + 1.0 * 0.5 = 0.5
        matrix.setMonoSourceValue(depth_src.index, 0.5f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        // depth_src = 1.0: identity
        // effective_depth = 0.0 + 1.0 * 1.0 = 1.0
        // result = 0.0 + 1.0 * 1.0 = 1.0
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
        REQUIRE(conn.depth_slot == 0);  // First connection gets slot 0
    }

    SECTION("Processing with source=1 and base=0 gives expected output") {
        matrix.setBaseValue(dst.index, 0.0f);
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();

        // result = 0.0 + 1.0 * 0.25 = 0.25
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.25f));
    }
}

TEST_CASE("F2: Adding same S->D again updates existing connection", "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn1 = matrix.addConnection(src, dst, 0.25f, false);
    uint16_t slot1 = conn1.depth_slot;

    auto conn2 = matrix.addConnection(src, dst, 0.75f, false);  // Same src, dst
    uint16_t slot2 = conn2.depth_slot;

    SECTION("Same slot is reused") {
        REQUIRE(slot1 == slot2);
    }

    SECTION("Depth is updated to new value") {
        matrix.setBaseValue(dst.index, 0.0f);
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();

        // result = 0.0 + 1.0 * 0.75 = 0.75 (new depth)
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.75f));
    }
}

TEST_CASE("F2b: Adding same depth mod again updates existing depth mod connection", "[modmatrix][connections]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, dst, 0.0f, false);  // base depth = 0

    auto depth_conn1 = matrix.addDepthModulation(depth_src, conn, 0.25f, false);
    uint16_t slot1 = depth_conn1.depth_slot;

    auto depth_conn2 = matrix.addDepthModulation(depth_src, conn, 0.75f, false);  // Same src, target
    uint16_t slot2 = depth_conn2.depth_slot;

    SECTION("Connection count remains 2 (no duplicate added)") {
        REQUIRE(matrix.getConnections().size() == 2);
    }

    SECTION("Same depth slot is reused") {
        REQUIRE(slot1 == slot2);
    }

    SECTION("Depth is updated to new value") {
        matrix.setBaseValue(dst.index, 0.0f);
        matrix.setMonoSourceValue(main_src.index, 1.0f);
        matrix.setMonoSourceValue(depth_src.index, 1.0f);
        matrix.process();

        // effective_depth = 0.0 + 1.0 * 0.75 = 0.75
        // result = 0.0 + 1.0 * 0.75 = 0.75
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.75f));
    }

    SECTION("Bipolar mapping flag is updated") {
        matrix.addDepthModulation(depth_src, conn, 0.5f, true);  // flip to bipolar
        REQUIRE(matrix.getConnections().size() == 2);
        // Find the depth mod and verify its bipolar flag
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

    auto conn = matrix.addConnection(main_src, dst, 0.0f, false);  // base depth = 0
    matrix.addDepthModulation(mod1, conn, 0.1f, false);
    matrix.addDepthModulation(mod2, conn, 0.2f, false);

    matrix.setBaseValue(dst.index, 0.0f);
    matrix.setMonoSourceValue(main_src.index, 1.0f);
    matrix.setMonoSourceValue(mod1.index, 1.0f);
    matrix.setMonoSourceValue(mod2.index, 0.5f);

    matrix.process();

    // effective_depth = 0.0 + 1.0*0.1 + 0.5*0.2 = 0.1 + 0.1 = 0.2
    // result = 0.0 + 1.0 * 0.2 = 0.2
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

    // MM connection - use immediately before any mutations
    auto mm_conn = matrix.addConnection(mono_src, mono_dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, mm_conn, 1.0f, false);

    // MP connection - use immediately before any mutations
    auto mp_conn = matrix.addConnection(mono_src, poly_dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, mp_conn, 1.0f, false);

    // PP connection - use immediately before any mutations
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
        // effective_depth = 0.0 + 0.5 * 1.0 = 0.5
        // result = 0.0 + 1.0 * 0.5 = 0.5
        REQUIRE(matrix.getModValue(mono_dst.index) == Catch::Approx(0.5f));
    }

    SECTION("MP connection affected by depth mod") {
        // Both MP and PP contribute to poly_dst for voice 0
        // MP: 0.0 + 1.0 * 0.5 = 0.5
        // PP: 0.0 + 1.0 * 0.5 = 0.5
        // Total = 0.0 + 0.5 + 0.5 = 1.0
        REQUIRE(matrix.getPolyModValue(poly_dst.index, 0) == Catch::Approx(1.0f));
    }
}

TEST_CASE("G1: Both-source toggling moves routes between buckets", "[modmatrix][toggle]")
{
    ModMatrix matrix(SmallConfig);

    auto& lfo = matrix.registerSource("LFO1", ModSrcType::Both, true, ModSrcMode::Mono);
    auto& cutoff = matrix.registerDestination("Cutoff", ModDstMode::Poly);

    matrix.addConnection(lfo, cutoff, 1.0f, true);  // bipolar mapping
    matrix.setBaseValue(cutoff.index, 0.5f);

    // Activate two voices
    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(1);

    SECTION("Mono mode: same modulation for all voices") {
        // LFO starts in Mono mode (default)
        matrix.setMonoSourceValue(lfo.index, 0.5f);  // normalized 0.75 -> bipolar 0.5
        matrix.process();

        float v0 = matrix.getPolyModValue(cutoff.index, 0);
        float v1 = matrix.getPolyModValue(cutoff.index, 1);
        REQUIRE(v0 == Catch::Approx(v1));  // Both voices get same modulation
    }

    SECTION("Poly mode: different modulation per voice") {
        matrix.setSourceMode(lfo.index, ModSrcMode::Poly);

        // Set different poly values per voice
        matrix.setPolySourceValue(lfo.index, 0, -1.0f);  // bipolar
        matrix.setPolySourceValue(lfo.index, 1, 1.0f);
        matrix.process();

        float v0 = matrix.getPolyModValue(cutoff.index, 0);
        float v1 = matrix.getPolyModValue(cutoff.index, 1);
        REQUIRE(v0 != Catch::Approx(v1));  // Voices have different modulation
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
        REQUIRE(v0 == Catch::Approx(0.5f));  // 0.0 + 1.0 * 0.5 = 0.5
    }

    SECTION("Poly mode: depth varies per voice") {
        matrix.setSourceMode(depth_src.index, ModSrcMode::Poly);
        matrix.setPolySourceValue(depth_src.index, 0, 0.2f);
        matrix.setPolySourceValue(depth_src.index, 1, 0.8f);
        matrix.process();

        float v0 = matrix.getPolyModValue(dst.index, 0);
        float v1 = matrix.getPolyModValue(dst.index, 1);
        REQUIRE(v0 == Catch::Approx(0.2f));  // depth = 0.0 + 0.2 * 1.0 = 0.2
        REQUIRE(v1 == Catch::Approx(0.8f));  // depth = 0.0 + 0.8 * 1.0 = 0.8
    }
}

TEST_CASE("G3: Dynamic mode toggle during execution", "[modmatrix][toggle]")
{
    ModMatrix matrix(SmallConfig);

    // Create a Both-type source (supports mono/poly toggle)
    auto& lfo = matrix.registerSource("LFO1", ModSrcType::Both, true, ModSrcMode::Mono);
    auto& cutoff = matrix.registerDestination("Cutoff", ModDstMode::Poly);

    matrix.addConnection(lfo, cutoff, 1.0f, true);  // bipolar mapping
    matrix.setBaseValue(cutoff.index, 0.5f);

    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(1);

    // Set BOTH mono and poly source values upfront
    // (simulates real scenario where both signal generators run continuously)
    matrix.setMonoSourceValue(lfo.index, 0.0f);      // mono LFO at center
    matrix.setPolySourceValue(lfo.index, 0, -1.0f);  // voice 0 poly LFO at min
    matrix.setPolySourceValue(lfo.index, 1, 1.0f);   // voice 1 poly LFO at max

    // --- Phase 1: Mono mode ---
    matrix.process();
    float v0_mono = matrix.getPolyModValue(cutoff.index, 0);
    float v1_mono = matrix.getPolyModValue(cutoff.index, 1);

    // In mono mode, both voices should receive the same modulation (from mono source)
    REQUIRE(v0_mono == Catch::Approx(v1_mono));

    // --- Phase 2: Toggle to Poly mode ---
    matrix.setSourceMode(lfo.index, ModSrcMode::Poly);
    matrix.process();
    float v0_poly = matrix.getPolyModValue(cutoff.index, 0);
    float v1_poly = matrix.getPolyModValue(cutoff.index, 1);

    // In poly mode, voices should receive different modulation (from per-voice sources)
    REQUIRE(v0_poly != Catch::Approx(v1_poly));

    // --- Phase 3: Toggle back to Mono mode ---
    matrix.setSourceMode(lfo.index, ModSrcMode::Mono);
    matrix.process();
    float v0_back = matrix.getPolyModValue(cutoff.index, 0);
    float v1_back = matrix.getPolyModValue(cutoff.index, 1);

    // Should return to identical modulation across voices
    REQUIRE(v0_back == Catch::Approx(v1_back));
    REQUIRE(v0_back == Catch::Approx(v0_mono));  // Same as original mono result
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
    REQUIRE(second == Catch::Approx(first));  // Same result, not accumulated
}

TEST_CASE("H2: Order independence within a bucket (commutativity)", "[modmatrix][determinism]")
{
    ModMatrix matrix(SmallConfig);

    auto& src1 = matrix.registerSource("src1", ModSrcType::Mono, false);
    auto& src2 = matrix.registerSource("src2", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    // Add connections (order: src1, src2)
    matrix.addConnection(src1, dst, 0.3f, false);
    matrix.addConnection(src2, dst, 0.2f, false);

    matrix.setBaseValue(dst.index, 0.0f);
    matrix.setMonoSourceValue(src1.index, 1.0f);
    matrix.setMonoSourceValue(src2.index, 1.0f);

    matrix.process();
    float result = matrix.getModValue(dst.index);

    // result = 0.0 + 1.0*0.3 + 1.0*0.2 = 0.5
    REQUIRE(result == Catch::Approx(0.5f));

    // Now create another matrix with reversed order
    ModMatrix matrix2(SmallConfig);
    auto& src2b = matrix2.registerSource("src2", ModSrcType::Mono, false);
    auto& src1b = matrix2.registerSource("src1", ModSrcType::Mono, false);
    auto& dstb = matrix2.registerDestination("dst", ModDstMode::Mono);

    // Add connections (reversed order: src2, src1)
    matrix2.addConnection(src2b, dstb, 0.2f, false);
    matrix2.addConnection(src1b, dstb, 0.3f, false);

    matrix2.setBaseValue(dstb.index, 0.0f);
    matrix2.setMonoSourceValue(src1b.index, 1.0f);
    matrix2.setMonoSourceValue(src2b.index, 1.0f);

    matrix2.process();
    float result2 = matrix2.getModValue(dstb.index);

    REQUIRE(result == Catch::Approx(result2));  // Same result regardless of order
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
    REQUIRE(handle.getValue() == Catch::Approx(0.5f));  // 0.25 + 0.5 * 0.5 = 0.5
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

    // PM connections are NYI - the connection is compiled to pm_connections
    // but the processing loop is empty (line 593 in ModMatrix.h)
    // So output should equal base value only
    REQUIRE(matrix.getModValue(mono_dst.index) == Catch::Approx(0.5f));
}

TEST_CASE("J2: Poly depth mod on MM slot is silently ignored", "[modmatrix][nyi]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Poly, false);  // Poly depth mod
    auto& mono_dst = matrix.registerDestination("mono_dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, mono_dst, 0.0f, false);  // MM connection
    matrix.addDepthModulation(depth_src, conn, 1.0f, false);

    matrix.setBaseValue(mono_dst.index, 0.0f);
    matrix.setMonoSourceValue(main_src.index, 1.0f);

    matrix.notifyVoiceOn(0);
    matrix.setPolySourceValue(depth_src.index, 0, 0.8f);

    matrix.process();

    // MM connections read from mono_depth_buf_, which only has mono depth mods applied.
    // Poly depth mods go to poly_depth_buf_, which MM connections don't use.
    // So the effective depth should be the base (0.0), and output should be 0.0
    REQUIRE(matrix.getModValue(mono_dst.index) == Catch::Approx(0.0f));
}

TEST_CASE("Edge: Empty matrix operations", "[modmatrix][edge]")
{
    ModMatrix matrix(SmallConfig);

    SECTION("process() with no sources/destinations doesn't crash") {
        matrix.process();
        REQUIRE(true);  // If we get here, no crash
    }

    SECTION("process() with sources but no destinations") {
        matrix.registerSource("src", ModSrcType::Mono);
        matrix.setMonoSourceValue(0, 0.5f);
        matrix.process();
        REQUIRE(true);
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

    // No voices activated
    matrix.process();

    // No active voices means no poly processing
    // Value at voice 0 depends on implementation (may be stale)
    REQUIRE(true);  // Just verify no crash
}

TEST_CASE("Edge: Negative depth values invert modulation", "[modmatrix][edge]")
{
    ModMatrix matrix(SmallConfig);

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    matrix.addConnection(src, dst, -0.5f, false);  // Negative depth
    matrix.setBaseValue(dst.index, 0.5f);
    matrix.setMonoSourceValue(src.index, 1.0f);

    matrix.process();

    // result = 0.5 + 1.0 * (-0.5) = 0.0
    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));
}

TEST_CASE("Edge: Zero base depth with depth modulation", "[modmatrix][edge]")
{
    ModMatrix matrix(SmallConfig);

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto conn = matrix.addConnection(main_src, dst, 0.0f, false);  // base depth = 0
    matrix.addDepthModulation(depth_src, conn, 1.0f, false);

    matrix.setBaseValue(dst.index, 0.0f);
    matrix.setMonoSourceValue(main_src.index, 1.0f);
    matrix.setMonoSourceValue(depth_src.index, 0.75f);

    matrix.process();

    // effective_depth = 0.0 + 0.75 * 1.0 = 0.75
    // result = 0.0 + 1.0 * 0.75 = 0.75
    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.75f));
}

TEST_CASE("K1: Oracle verification for simple mono patch", "[modmatrix][oracle]")
{
    // Simple test case: mono source -> mono destination
    // Verify ModMatrix output matches oracle exactly
    ModMatrix matrix(SmallConfig);
    ModMatrixOracle oracle(4, 8, 16);

    // Register identical sources and destinations
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);  // unipolar
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);
    oracle.addSource(true, false, false);  // mono, not both, unipolar
    oracle.addDestination(true);  // mono

    // Add connection with same parameters
    matrix.addConnection(src, dst, 0.5f, false);  // unipolar mapping
    oracle.addConnection(0, 0, 0.5f, false);

    // Set values
    matrix.setBaseValue(dst.index, 0.25f);
    oracle.setBaseValue(0, 0.25f);  // directly set normalized (0.25 for 0..1 range)

    matrix.setMonoSourceValue(src.index, 0.6f);
    oracle.setMonoSource(0, 0.6f);

    // Process
    matrix.process();
    auto [oracle_mono, oracle_poly] = oracle.process();

    // result = 0.25 + 0.6 * 0.5 = 0.55
    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.55f));
    REQUIRE(oracle_mono[0] == Catch::Approx(0.55f));
    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(oracle_mono[0]));
}

TEST_CASE("K1: Oracle verification for poly patch", "[modmatrix][oracle]")
{
    ModMatrix matrix(SmallConfig);
    ModMatrixOracle oracle(4, 8, 16);

    // Poly source -> Poly destination
    auto& src = matrix.registerSource("src", ModSrcType::Poly, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);
    oracle.addSource(false, false, false);  // poly
    oracle.addDestination(false);  // poly

    matrix.addConnection(src, dst, 1.0f, false);
    oracle.addConnection(0, 0, 1.0f, false);

    matrix.setBaseValue(dst.index, 0.0f);
    oracle.setBaseValue(0, 0.0f);

    // Activate voice 0 and 2
    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(2);
    oracle.active_voices = {0, 2};

    // Set different poly values per voice
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

    // Bipolar source -> mono destination with bipolar mapping
    auto& src = matrix.registerSource("src", ModSrcType::Mono, true);  // bipolar
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);
    oracle.addSource(true, false, true);  // bipolar source
    oracle.addDestination(true);

    matrix.addConnection(src, dst, 1.0f, true);  // bipolar mapping
    oracle.addConnection(0, 0, 1.0f, true);

    matrix.setBaseValue(dst.index, 0.5f);
    oracle.setBaseValue(0, 0.5f);

    // Test with bipolar input = 0 (should map to 0 contribution)
    matrix.setMonoSourceValue(src.index, 0.0f);
    oracle.setMonoSource(0, 0.0f);

    matrix.process();
    auto [oracle_mono, oracle_poly] = oracle.process();

    // bipolar 0.0 -> normalized 0.5 -> bipolar output 0.0
    // result = 0.5 + 0.0 * 1.0 = 0.5
    REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));
    REQUIRE(oracle_mono[0] == Catch::Approx(0.5f));
}

TEST_CASE("K1: Oracle verification for MP connection", "[modmatrix][oracle]")
{
    ModMatrix matrix(SmallConfig);
    ModMatrixOracle oracle(4, 8, 16);

    // Mono source -> Poly destination (MP)
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);
    oracle.addSource(true, false, false);
    oracle.addDestination(false);  // poly

    matrix.addConnection(src, dst, 0.5f, false);
    oracle.addConnection(0, 0, 0.5f, false);

    matrix.setBaseValue(dst.index, 0.2f);
    oracle.setBaseValue(0, 0.2f);

    matrix.notifyVoiceOn(0);
    matrix.notifyVoiceOn(1);
    oracle.active_voices = {0, 1};

    // Mono source value applies to all active voices
    matrix.setMonoSourceValue(src.index, 0.4f);
    oracle.setMonoSource(0, 0.4f);

    matrix.process();
    auto [oracle_mono, oracle_poly] = oracle.process();

    // result = 0.2 + 0.4 * 0.5 = 0.4
    float expected = 0.4f;
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

    // Remove depth mod using the reference overload
    bool removed = matrix.removeConnection(depth_conn);
    REQUIRE(removed);
    REQUIRE(matrix.getConnections().size() == 1);

    // Verify the main connection is still there
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

// ---------------------------------------------------------------------------
// Section M: Modulation offset range tests
// ---------------------------------------------------------------------------

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
    // Peak-to-peak of a bipolar-mapped connection equals the depth magnitude, so the offset
    // range splits symmetrically into ±|d|/2.
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, true);  // bipolar source
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

    matrix.addConnection(s_bi, dst, 0.2f, true);       // bipolar   -> [-0.1, +0.1]
    matrix.addConnection(s_up_pos, dst, 0.3f, false);  // unipolar  -> [0,    +0.3]
    matrix.addConnection(s_up_neg, dst, -0.1f, false); // unipolar  -> [-0.1,  0 ]

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

    // Adding depth modulation on conn should NOT change dst's static range.
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
    // Cross-check: with sources pinned to their extreme values, process() output minus base
    // must fall within the reported [min, max]. Guards against drift if process()'s
    // per-connection contribution math is ever changed.
    ModMatrix matrix(SmallConfig);
    auto& s_bi = matrix.registerSource("s_bi", ModSrcType::Mono, true);
    auto& s_up = matrix.registerSource("s_up", ModSrcType::Mono, false);

    // Use a linear 0..1 scale so plain == normalized; no un-scaling math needed.
    applause::ValueScaleInfo identity{0.0f, 1.0f, applause::ValueScaling::linear()};
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);

    matrix.addConnection(s_bi, dst, 0.3f, true);    // [-0.15, +0.15]
    matrix.addConnection(s_up, dst, 0.2f, false);   // [  0,   +0.20]

    const float base = 0.5f;
    matrix.setBaseValue(dst.index, base);

    auto [min_off, max_off] = matrix.getModOffsetRange(dst.index);
    REQUIRE(min_off == Catch::Approx(-0.15f));
    REQUIRE(max_off == Catch::Approx(+0.35f));

    // Drive toward the reported maximum: s_bi=+1 (full positive bipolar), s_up=+1.
    matrix.setMonoSourceValue(s_bi.index, +1.0f);
    matrix.setMonoSourceValue(s_up.index, +1.0f);
    matrix.process();
    float out_max_offset = matrix.getModValue(dst.index) - base;
    REQUIRE(out_max_offset <= max_off + 1e-5f);

    // Drive toward the reported minimum: s_bi=-1, s_up=0 (unipolar min).
    matrix.setMonoSourceValue(s_bi.index, -1.0f);
    matrix.setMonoSourceValue(s_up.index, 0.0f);
    matrix.process();
    float out_min_offset = matrix.getModValue(dst.index) - base;
    REQUIRE(out_min_offset >= min_off - 1e-5f);
}

// ---------------------------------------------------------------------------
// Section N: Smart default mapping in addConnection
// ---------------------------------------------------------------------------
// When the caller omits `bipolar_mapping`, addConnection infers a sensible default:
//   - bipolar sources          → bipolar-mapped (unchanged from before)
//   - unipolar sources, base near center (1/3..2/3) → bipolar-mapped
//   - unipolar sources, base near min  (<1/3)       → unipolar-mapped, depth forced positive
//   - unipolar sources, base near max  (>2/3)       → unipolar-mapped, depth forced negative
// Explicit `bipolar_mapping` always overrides this.

TEST_CASE("N1: Bipolar source defaults to bipolar-mapped regardless of base",
          "[modmatrix][connections][default_mapping]")
{
    auto check = [](float base) {
        ModMatrix matrix(SmallConfig);
        auto& src = matrix.registerSource("src", ModSrcType::Mono, true);  // bipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);
        matrix.setBaseValue(dst.index, base);
        auto conn = matrix.addConnection(src, dst, 0.5f);  // no explicit mapping
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
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);  // unipolar
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);
    matrix.setBaseValue(dst.index, 0.1f);

    // Pass a negative depth; resolver should sign-flip it to positive.
    auto conn = matrix.addConnection(src, dst, -0.8f);
    REQUIRE(!conn.isBipolar());
    REQUIRE(conn.getDepth() == Catch::Approx(+0.8f));
}

TEST_CASE("N3: Unipolar source, base near center → bipolar-mapped",
          "[modmatrix][connections][default_mapping]")
{
    applause::ValueScaleInfo identity{0.0f, 1.0f, applause::ValueScaling::linear()};
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);  // unipolar
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);
    matrix.setBaseValue(dst.index, 0.5f);

    auto conn = matrix.addConnection(src, dst, 0.6f);
    REQUIRE(conn.isBipolar());
    REQUIRE(conn.getDepth() == Catch::Approx(0.6f));  // depth preserved when center-mapped
}

TEST_CASE("N4: Unipolar source, base near max → unipolar-mapped, depth forced negative",
          "[modmatrix][connections][default_mapping]")
{
    applause::ValueScaleInfo identity{0.0f, 1.0f, applause::ValueScaling::linear()};
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);  // unipolar
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity);
    matrix.setBaseValue(dst.index, 0.9f);

    // Pass positive depth; resolver should sign-flip it toward the interior (negative).
    auto conn = matrix.addConnection(src, dst, 0.8f);
    REQUIRE(!conn.isBipolar());
    REQUIRE(conn.getDepth() == Catch::Approx(-0.8f));
}

TEST_CASE("N5: Explicit bipolar_mapping overrides the smart default",
          "[modmatrix][connections][default_mapping]")
{
    ModMatrix matrix(SmallConfig);
    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);  // unipolar
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);
    matrix.setBaseValue(dst.index, 0.0f);  // would normally pick unipolar+

    auto conn = matrix.addConnection(src, dst, 0.5f, true);  // explicit bipolar override
    REQUIRE(conn.isBipolar());
    REQUIRE(conn.getDepth() == Catch::Approx(0.5f));  // depth not sign-normalized
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
