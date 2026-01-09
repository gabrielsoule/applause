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

using SmallMatrix = ModMatrix<4, 8, 16, 32>;       // Simple tests
using StandardMatrix = ModMatrix<16, 32, 64, 128>; // Realistic synth
using MinimalMatrix = ModMatrix<1, 1, 1, 1>;       // Edge cases

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

    struct Connection {
        uint16_t src_idx;
        uint16_t dst_idx;
        uint16_t depth_slot;
        bool bipolar_mapping;
    };

    struct DepthConnection {
        uint16_t src_idx;
        uint16_t depth_slot;
        float depth;
        bool bipolar_mapping;
    };

    std::vector<Source> sources;
    std::vector<Destination> destinations;
    std::vector<Connection> connections;
    std::vector<DepthConnection> depth_connections;
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
        connections.push_back({src, dst, slot, bipolar_mapping});
        return slot;
    }

    void addDepthModulation(uint16_t src, uint16_t slot, float depth, bool bipolar_mapping) {
        depth_connections.push_back({src, slot, depth, bipolar_mapping});
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
     * Implements the exact bipolar normalization logic from ModMatrix::process()
     */
    static float applyBipolarNormalization(float src_val, bool src_bipolar, bool bipolar_mapping) {
        if (src_bipolar) {
            src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
        }
        if (bipolar_mapping) {
            src_val = src_val * 2.0f - 1.0f;    // [0,1] -> [-1,+1]
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

        // Mono depth modulation
        for (const auto& dc : depth_connections) {
            if (effectivelyMono(dc.src_idx)) {
                float src_val = mono_src_values[dc.src_idx];
                src_val = applyBipolarNormalization(src_val, sources[dc.src_idx].bipolar, dc.bipolar_mapping);
                mono_depth[dc.depth_slot] += src_val * dc.depth;
            }
        }

        // Copy mono depth to poly, apply poly depth modulation
        for (uint16_t v : active_voices) {
            poly_depth[v] = mono_depth;
            for (const auto& dc : depth_connections) {
                if (!effectivelyMono(dc.src_idx)) {
                    float src_val = poly_src_values[v][dc.src_idx];
                    src_val = applyBipolarNormalization(src_val, sources[dc.src_idx].bipolar, dc.bipolar_mapping);
                    poly_depth[v][dc.depth_slot] += src_val * dc.depth;
                }
            }
        }

        // Apply connections
        for (const auto& conn : connections) {
            bool src_mono = effectivelyMono(conn.src_idx);
            bool dst_mono = destinations[conn.dst_idx].is_mono;
            bool src_bipolar = sources[conn.src_idx].bipolar;

            if (src_mono && dst_mono) {
                // MM
                float src_val = mono_src_values[conn.src_idx];
                src_val = applyBipolarNormalization(src_val, src_bipolar, conn.bipolar_mapping);
                mono_out[conn.dst_idx] += src_val * mono_depth[conn.depth_slot];
            }
            else if (src_mono && !dst_mono) {
                // MP
                for (uint16_t v : active_voices) {
                    float src_val = mono_src_values[conn.src_idx];
                    src_val = applyBipolarNormalization(src_val, src_bipolar, conn.bipolar_mapping);
                    poly_out[v][conn.dst_idx] += src_val * poly_depth[v][conn.depth_slot];
                }
            }
            else if (!src_mono && !dst_mono) {
                // PP
                for (uint16_t v : active_voices) {
                    float src_val = poly_src_values[v][conn.src_idx];
                    src_val = applyBipolarNormalization(src_val, src_bipolar, conn.bipolar_mapping);
                    poly_out[v][conn.dst_idx] += src_val * poly_depth[v][conn.depth_slot];
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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

    // Should not crash
    matrix.notifyVoiceOff(0);
    matrix.notifyVoiceOff(1);
    matrix.notifyVoiceOff(3);

    // No assertion/crash means test passes
    REQUIRE(true);
}

TEST_CASE("C1: With no connections, output equals base plain value after scaling", "[modmatrix][scaling]")
{
    SmallMatrix matrix;

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
    SmallMatrix matrix;

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

    SmallMatrix matrix;

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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

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

    SECTION("src_bipolar=true, bipolar_mapping=true (identity through both transforms)") {
        SmallMatrix matrix;
        auto& src = matrix.registerSource("src", ModSrcType::Mono, true);  // bipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);
        matrix.addConnection(src, dst, 1.0f, true);  // bipolar mapping
        matrix.setBaseValue(dst.index, 0.5f);

        // Input -1.0: [-1,+1] -> [0,1] = 0.0 -> [-1,+1] = -1.0
        // result = 0.5 + (-1.0) * 1.0 = -0.5 -> clamped to 0
        matrix.setMonoSourceValue(src.index, -1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        // Input 0.0: [-1,+1] -> [0,1] = 0.5 -> [-1,+1] = 0.0
        // result = 0.5 + 0.0 * 1.0 = 0.5
        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        // Input +1.0: [-1,+1] -> [0,1] = 1.0 -> [-1,+1] = +1.0
        // result = 0.5 + 1.0 * 1.0 = 1.5 -> clamped to 1.0
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("src_bipolar=true, bipolar_mapping=false (half-wave rectify)") {
        SmallMatrix matrix;
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

    SECTION("src_bipolar=false, bipolar_mapping=true (expand to bipolar)") {
        SmallMatrix matrix;
        auto& src = matrix.registerSource("src", ModSrcType::Mono, false);  // unipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);
        matrix.addConnection(src, dst, 1.0f, true);  // bipolar mapping
        matrix.setBaseValue(dst.index, 0.5f);

        // Input 0.0: no normalization (already [0,1]) -> [-1,+1] = -1.0
        // result = 0.5 + (-1.0) * 1.0 = -0.5 -> clamped to 0
        matrix.setMonoSourceValue(src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        // Input 0.5: -> [-1,+1] = 0.0
        // result = 0.5 + 0.0 * 1.0 = 0.5
        matrix.setMonoSourceValue(src.index, 0.5f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        // Input 1.0: -> [-1,+1] = +1.0
        // result = 0.5 + 1.0 * 1.0 = 1.5 -> clamped to 1.0
        matrix.setMonoSourceValue(src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("src_bipolar=false, bipolar_mapping=false (identity)") {
        SmallMatrix matrix;
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
        SmallMatrix matrix;
        auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
        auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, true);  // bipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);

        auto& conn = matrix.addConnection(main_src, dst, 0.0f, false);  // base depth = 0
        matrix.addDepthModulation(depth_src, conn.depth_slot, 1.0f, true);  // bipolar mapping
        matrix.setBaseValue(dst.index, 0.5f);
        matrix.setMonoSourceValue(main_src.index, 1.0f);

        // depth_src = -1.0: normalized to 0.0, bipolar_mapping to -1.0
        // effective_depth = 0.0 + (-1.0) * 1.0 = -1.0
        // result = 0.5 + 1.0 * (-1.0) = -0.5 -> clamped to 0
        matrix.setMonoSourceValue(depth_src.index, -1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.0f));

        // depth_src = 0.0: normalized to 0.5, bipolar_mapping to 0.0
        // effective_depth = 0.0 + 0.0 * 1.0 = 0.0
        // result = 0.5 + 1.0 * 0.0 = 0.5
        matrix.setMonoSourceValue(depth_src.index, 0.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(0.5f));

        // depth_src = +1.0: normalized to 1.0, bipolar_mapping to +1.0
        // effective_depth = 0.0 + 1.0 * 1.0 = 1.0
        // result = 0.5 + 1.0 * 1.0 = 1.5 -> clamped to 1.0
        matrix.setMonoSourceValue(depth_src.index, 1.0f);
        matrix.process();
        REQUIRE(matrix.getModValue(dst.index) == Catch::Approx(1.0f));
    }

    SECTION("src_bipolar=false, bipolar_mapping=false for depth mod (identity)") {
        SmallMatrix matrix;
        auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
        auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);  // unipolar
        auto& dst = matrix.registerDestination("dst", ModDstMode::Mono, identity_scale);

        auto& conn = matrix.addConnection(main_src, dst, 0.0f, false);
        matrix.addDepthModulation(depth_src, conn.depth_slot, 1.0f, false);  // unipolar mapping
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
    SmallMatrix matrix;

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto& conn = matrix.addConnection(src, dst, 0.25f, false);

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
    SmallMatrix matrix;

    auto& src = matrix.registerSource("src", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto& conn1 = matrix.addConnection(src, dst, 0.25f, false);
    uint16_t slot1 = conn1.depth_slot;

    auto& conn2 = matrix.addConnection(src, dst, 0.75f, false);  // Same src, dst
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

TEST_CASE("F3: Multiple depth mod routes to same depth slot sum", "[modmatrix][connections]")
{
    SmallMatrix matrix;

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& mod1 = matrix.registerSource("mod1", ModSrcType::Mono, false);
    auto& mod2 = matrix.registerSource("mod2", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto& conn = matrix.addConnection(main_src, dst, 0.0f, false);  // base depth = 0
    matrix.addDepthModulation(mod1, conn.depth_slot, 0.1f, false);
    matrix.addDepthModulation(mod2, conn.depth_slot, 0.2f, false);

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
    SmallMatrix matrix;

    auto& mono_src = matrix.registerSource("mono_src", ModSrcType::Mono, false);
    auto& poly_src = matrix.registerSource("poly_src", ModSrcType::Poly, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);
    auto& mono_dst = matrix.registerDestination("mono_dst", ModDstMode::Mono);
    auto& poly_dst = matrix.registerDestination("poly_dst", ModDstMode::Poly);

    // MM connection
    auto& mm_conn = matrix.addConnection(mono_src, mono_dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, mm_conn.depth_slot, 1.0f, false);

    // MP connection
    auto& mp_conn = matrix.addConnection(mono_src, poly_dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, mp_conn.depth_slot, 1.0f, false);

    // PP connection
    auto& pp_conn = matrix.addConnection(poly_src, poly_dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, pp_conn.depth_slot, 1.0f, false);

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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Both, false, ModSrcMode::Mono);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Poly);

    auto& conn = matrix.addConnection(main_src, dst, 0.0f, false);
    matrix.addDepthModulation(depth_src, conn.depth_slot, 1.0f, false);

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

TEST_CASE("H1: Outputs do not accumulate across blocks", "[modmatrix][determinism]")
{
    SmallMatrix matrix;

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
    SmallMatrix matrix;

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
    SmallMatrix matrix2;
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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Poly, false);  // Poly depth mod
    auto& mono_dst = matrix.registerDestination("mono_dst", ModDstMode::Mono);

    auto& conn = matrix.addConnection(main_src, mono_dst, 0.0f, false);  // MM connection
    matrix.addDepthModulation(depth_src, conn.depth_slot, 1.0f, false);

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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

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
    SmallMatrix matrix;

    auto& main_src = matrix.registerSource("main", ModSrcType::Mono, false);
    auto& depth_src = matrix.registerSource("depth", ModSrcType::Mono, false);
    auto& dst = matrix.registerDestination("dst", ModDstMode::Mono);

    auto& conn = matrix.addConnection(main_src, dst, 0.0f, false);  // base depth = 0
    matrix.addDepthModulation(depth_src, conn.depth_slot, 1.0f, false);

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
    SmallMatrix matrix;
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
    SmallMatrix matrix;
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
    SmallMatrix matrix;
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
    SmallMatrix matrix;
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
