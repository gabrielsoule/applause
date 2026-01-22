#pragma once

#include <algorithm>
#include <applause/extensions/ParamsExtension.h>
#include <applause/util/DebugHelpers.h>
#include <applause/util/ValueScaling.h>
#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace applause {

enum class ModSrcType : uint8_t { Mono, Poly, Both };

enum class ModSrcMode : uint8_t { Mono, Poly };

enum class ModDstMode : uint8_t { Mono, Poly };

/**
 * Represents a logical modulation source. A modulation source can be monophonic, polyphonic, or it can support both
 * modes of operation via a toggle
 * (e.g. a LFO in a synth may have a "mono" UI toggle which would map to the mod source mode).
 *
 * In practice, a ModSrcId is a logical representation of several modulation signal generators that may appear to the
 * user as a single modulator: for example, a polyphonic LFO in an N-voice synthesizer would need N distinct LFO signal
 * generators (one per voice) plus an additional generator for the mono LFO signal if a mono toggle is supported as
 * described above, for a total of N + 1 "channels".
 *
 * The ModSrcType abstraction is designed specifically to support the foregoing scenario.
 *
 * The bipolar flag indicates the native output range of the source:
 * - bipolar=true:  Source outputs [-1, +1] (e.g., LFO, pitch bend)
 * - bipolar=false: Source outputs [0, 1]   (e.g., envelope, velocity)
 */
struct ModSource {
    std::string name;
    uint16_t index;
    ModSrcType type;
    ModSrcMode mode;
    bool bipolar;  ///< True if source naturally outputs [-1,+1], false for [0,1]
};

struct ModDestination {
    std::string name;
    uint16_t index;
    ModDstMode mode;
};

class ModMatrix;  // Forward declaration for ModConnection

/**
 * Unified modulation connection structure.
 * Holds a pointer to its parent ModMatrix for safe depth access (avoids dangling pointers
 * that could occur if we stored raw pointers into reallocating vectors).
 */
struct ModConnection {
    ModMatrix* matrix_ = nullptr;  ///< Parent matrix (required for depth/recompile access)
    uint16_t src_idx = 0;  ///< Source index
    uint16_t dst_idx = 0;  ///< Destination index (param conn) OR target depth slot (depth mod)
    uint16_t depth_slot = 0;  ///< Slot index where this connection's depth is stored
    uint8_t flags = 0;  ///< Packed flags: bit 0 = is_depth_mod, bit 1 = bipolar_mapping

    [[nodiscard]] bool isDepthMod() const { return flags & 0x01; }
    [[nodiscard]] bool isBipolar() const { return flags & 0x02; }

    // Implemented after ModMatrix definition (require access to matrix internals)
    [[nodiscard]] float getDepth() const;
    void setDepth(float d);
    void setBipolar(bool v);

    // Convenience accessors
    [[nodiscard]] const ModSource& source() const;
    [[nodiscard]] const ModDestination* destination() const;  // nullptr for depth mods
};

/**
 * Compiled connection handle for efficient real-time processing.
 */
struct ModConnectionHandle {
    uint16_t src;  ///< Source index
    uint16_t target;  ///< Destination index (param conn) OR target depth slot (depth mod)
    uint16_t depth_slot;  ///< Slot index where this connection's depth is stored
    uint8_t flags;  ///< Packed flags

    [[nodiscard]] bool isDepthMod() const { return flags & 0x01; }
    [[nodiscard]] bool isSourceBipolar() const { return flags & 0x02; }
    [[nodiscard]] bool isBipolar() const { return flags & 0x04; }
};

/**
 * Lightweight handle for audio-thread access to a modulated parameter value.
 * Use getModHandle() to obtain. The pointer is pre-computed, so getValue()
 * is a simple dereference with no arithmetic.
 */
struct ModParamHandle {
    float* value_ = nullptr;
    [[nodiscard]] float getValue() const noexcept { return *value_; }
};

/**
 * !!! WIP !!!
 *
 * A fast and efficient parameter modulation system. Supports modulation from mod sources (e.g. LFOs) to destinations
 * (e.g. synth parameters). The depth of individual connections themselves can also be modulated;
 * in other words, the matrix supports depth-one modulation-of-modulation.
 *
 * The modulation system is designed independently of the Applause parameter module. While there is a natural
 * bijection between plugin parameters and modulation destinations, and we supply several helper functions that
 * interface between applause::ParamsExtension and applause::ModMatrix, no such association is strictly necessary.
 *
 * The current implementation is straightforward in the interest of performance and easy debugging.
 * The depth-one recursive modulation is baked into the system; modulation connections ("depth connections")
 * that modulate existing connections between sources and destinations cannot, themselves, be modulated.
 * We sacrifice generality upon the altar of pragmatism.
 *
 * Deeper modulation graphs would necessitate development of a signal digraph processing system, a graph compiler,
 * loop detection & resolution, et cetera. We omit this for now, as deeper recursive modulation (i.e. modulation of a
 * connection that modulates a connection that modulates a parameter) is an edge case not required within most synthesis
 * applications.
 *
 * This might be nice in the future, and a "ModMatrix 2.0" is on the distant roadmap.
 *
 */

/**
 * Represents a "compiled" modulation graph that can be executed efficiently. A ModMatrix owns exactly one of these.
 * When the user changes the modulation graph, this object is modified/replaced.
 *
 * We keep it as a separate object to atomically swap the whole thing all at the same time when an update is made,
 * so the DSP thread never reads from a partially-updated modulation system. Thread safety!
 *
 * (the actual atomic swapping shenanigans are NYI, but it's a TODO)
 */
class ModProgram {
    [[maybe_unused]] ModMatrix& matrix;

    std::vector<ModConnectionHandle> mm_connections;  // mono src -> mono dst
    std::vector<ModConnectionHandle> mp_connections;  // mono src -> poly dst
    std::vector<ModConnectionHandle> pm_connections;  // poly src -> mono dst (NYI)
    std::vector<ModConnectionHandle> pp_connections;  // poly src -> poly dst

    std::vector<float> depth_base_;
    std::vector<uint8_t> depth_active_;

    std::vector<ModConnectionHandle> depth_connections_mono_;
    std::vector<ModConnectionHandle> depth_connections_poly_;

public:
    explicit ModProgram(ModMatrix& matrix, uint16_t max_connections) : matrix(matrix) {
        mm_connections.reserve(max_connections);
        mp_connections.reserve(max_connections);
        pm_connections.reserve(max_connections);
        pp_connections.reserve(max_connections);
        depth_base_.reserve(max_connections);
        depth_active_.reserve(max_connections);
        depth_connections_mono_.reserve(max_connections);
        depth_connections_poly_.reserve(max_connections);
    }

    friend class ModMatrix;
};


class ModMatrix {
public:
    struct Config {
        uint16_t num_voices;
        uint16_t max_sources;
        uint16_t max_destinations;
        uint16_t max_connections;
    };

    explicit ModMatrix(Config config) :
        config_(config),
        poly_src_stride_(config.max_sources),
        poly_dst_stride_(config.max_destinations),
        poly_depth_stride_(config.max_connections),
        program_(*this, config.max_connections),
        src_registry_(config.max_sources),
        dst_registry_(config.max_destinations),
        dst_scale_info_(config.max_destinations),
        mono_src_buf_(config.max_sources, 0.0f),
        poly_src_buf_(static_cast<size_t>(config.num_voices) * config.max_sources, 0.0f),
        base_mono_dst_(config.max_destinations, 0.0f),
        base_poly_dst_(config.max_destinations, 0.0f),
        mono_depth_buf_(config.max_connections, 0.0f),
        poly_depth_buf_(static_cast<size_t>(config.num_voices) * config.max_connections, 0.0f),
        mono_dst_(config.max_destinations, 0.0f),
        poly_dst_buf_(static_cast<size_t>(config.num_voices) * config.max_destinations, 0.0f) {}

    ModMatrix() = delete;
    /**
     * Registers a new modulation source symbol uniquely identifiable via string_id. This function only registers the
     * source symbolically within the matrix; to link modulation channels to your signal generators (LFOs, envelopes,
     * etc) you must invoke registerMonoSourceChannel/registerPolySourceChannel.
     * @param string_id the unique identifier of the source
     * @param type whether the source is mono, poly, or supports both modes
     * @param bipolar true if source outputs [-1,+1] (e.g., LFO), false for [0,1] (e.g., envelope)
     * @param defaultMode default mode for sources that support both mono and poly
     * @return reference to the registered source
     */
    ModSource& registerSource(const std::string& string_id, ModSrcType type, bool bipolar = false,
                              ModSrcMode defaultMode = ModSrcMode::Poly);

    /**
     * Sets the mode for a source that supports both mono and poly operation.
     * This triggers a recompilation of the modulation program to update connection routing.
     * @param srcIdx The source index
     * @param mode The new mode (Mono or Poly)
     * @note Only valid for sources registered with ModSrcType::Both
     */
    void setSourceMode(uint16_t srcIdx, ModSrcMode mode);

    [[nodiscard]] uint16_t getSourceCount() const { return static_cast<uint16_t>(src_count_); }

    [[nodiscard]] const ModSource& getSource(uint16_t idx) const {
        ASSERT(idx < src_count_, "Source index out of bounds");
        return src_registry_[idx];
    }

    /**
     * Registers a new modulation destination.
     * @param string_id unique identifier for the destination
     * @param mode whether the destination is mono or poly
     * @param scale_info scaling info for converting normalized <-> real values. Optional; defaults to no scaling.
     * @return reference to the registered destination
     */
    ModDestination& registerDestination(const std::string& string_id, ModDstMode mode,
                                        applause::ValueScaleInfo scale_info = {0.0f, 1.0f,
                                                                               applause::ValueScaling::linear()});

    [[nodiscard]] uint16_t getDestinationCount() const { return static_cast<uint16_t>(dst_count_); }

    [[nodiscard]] const ModDestination& getDestination(uint16_t idx) const {
        ASSERT(idx < dst_count_, "Destination index out of bounds");
        return dst_registry_[idx];
    }

    /**
     * Helper function to automatically register all parameters from the given extension instance as modulation
     * destinations. Destination indices will be assigned monotonically from zero with respect to the order by which
     * the parameters are defined within the extension.
     *
     * This function automatically uses the scaling info from each parameter, so modulated values
     * will be returned in plain units ready for DSP use.
     *
     * IMPORTANT: This function assumes a 1:1 bijection between parameter indices and destination indices
     * (i.e., param 0 becomes destination 0, param 1 becomes destination 1, etc.). This assumption is
     * used by loadParamBaseValues() for efficient lookups. Therefore, this function should only be
     * called on an empty ModMatrix with no previously registered destinations.
     * Calling it after manually registering destinations will cause an index offset mismatch.
     *
     * This fragility can/should be patched in the future, but it hasn't been done yet!
     *
     * If you wish to add destinations that do not correspond to plugin parameters alongside plugin parameters,
     * please add them after calling this function.
     */
    void registerFromParamsExtension(const applause::ParamsExtension& params_extension);

    /**
     * Creates a new modulation connection between a source and a destination. If an existing connection exists, it will
     * be replaced.
     *
     * @param src the modulation source
     * @param dst the modulation destination
     * @param depth the initial modulation depth
     * @param bipolar_mapping whether the output should be centered at 0 (bidirectional).
     *        If not specified, defaults to the source's bipolar flag
     * @return reference to the connection
     */
    ModConnection& addConnection(ModSource src, ModDestination dst, float depth,
                                 std::optional<bool> bipolar_mapping = std::nullopt);

    /**
     * Removes a modulation connection between a source and destination.
     *
     * @param srcIdx the source index
     * @param dstIdx the destination index
     * @return true if a connection was found and removed, false otherwise
     */
    bool removeConnection(uint16_t srcIdx, uint16_t dstIdx);

    /**
     * Removes a modulation connection.
     */
    bool removeConnection(const ModConnection& connection);

    [[nodiscard]] const std::vector<ModConnection>& getConnections() const { return connections_; }

    [[nodiscard]] ModSource* findSource(const std::string& name);

    [[nodiscard]] const ModSource* findSource(const std::string& name) const;

    [[nodiscard]] ModDestination* findDestination(const std::string& name);

    [[nodiscard]] const ModDestination* findDestination(const std::string& name) const;

    /**
     * Adds a modulation connection that modulates the depth of an existing connection.
     *
     * @param src the modulation source
     * @param target_conn the connection whose depth should be modulated
     * @param depth the modulation depth for this depth-modulation connection
     * @param bipolar_mapping whether the output should be centered at 0 (bidirectional).
     *        If not specified, defaults to the source's bipolar flag.
     * @return reference to the depth modulation connection
     */
    ModConnection& addDepthModulation(ModSource src, const ModConnection& target_conn, float depth,
                                      std::optional<bool> bipolar_mapping = std::nullopt);

    /**
     * Notifies the matrix that the voice corresponding to voice_index has just been activated.
     * This function must be called whenever a voice is triggered by, e.g., a fresh incoming MIDI note
     *
     * The matrix uses this information to selectively
     * process polyphonic modulation paths, as well as handle poly->mono source->destination routing policy.
     */
    void notifyVoiceOn(uint16_t voice_index);

    /**
     * Notifies the matrix that the voice corresponding to voice_index has been deactivated and is no longer
     * being processed nor producing audio. This function must be called whenever a voice is disabled.
     */
    void notifyVoiceOff(uint16_t voice_index) { std::erase(active_voices_, voice_index); }

    /**
     * Sets the base (unmodulated) value for a destination in plain units.
     * The value is internally normalized using the destination's scaling info.
     */
    void setBaseValue(uint16_t dstIdx, float plain_value);

    /**
     * Sets the value for a mono modulation source.
     * Call this before process() to update source values from your modulators.
     * @param srcIdx The source index
     * @param value The source value ([-1,+1] for bipolar, [0,1] for unipolar)
     */
    void setMonoSourceValue(uint16_t srcIdx, float value) {
        ASSERT(srcIdx < src_count_, "Source index out of bounds");
        mono_src_buf_[srcIdx] = value;
    }

    /**
     * Sets the value for a poly modulation source for a specific voice.
     * Call this before process() to update source values from your modulators.
     * @param srcIdx The source index
     * @param voice The voice index
     * @param value The source value ([-1,+1] for bipolar, [0,1] for unipolar)
     */
    void setPolySourceValue(uint16_t srcIdx, uint16_t voice, float value) {
        ASSERT(srcIdx < src_count_, "Source index out of bounds");
        ASSERT(voice < config_.num_voices, "Voice index out of bounds");
        poly_src_buf_[static_cast<size_t>(voice) * poly_src_stride_ + srcIdx] = value;
    }

    /**
     * Sets both mono and poly values for a source simultaneously.
     * Useful for sources with ModSrcType::Both where both buffers should be updated.
     * @param srcIdx The source index
     * @param voice The voice index for the poly value
     * @param value The source value
     */
    void setSourceValue(uint16_t srcIdx, uint16_t voice, float value) {
        ASSERT(srcIdx < src_count_, "Source index out of bounds");
        ASSERT(voice < config_.num_voices, "Voice index out of bounds");
        mono_src_buf_[srcIdx] = value;
        poly_src_buf_[static_cast<size_t>(voice) * poly_src_stride_ + srcIdx] = value;
    }

    /**
     * Returns the final modulated value for a mono destination in plain units.
     * Call this after process() to retrieve the modulated parameter value.
     */
    [[nodiscard]] float getModValue(uint16_t dstIdx) const {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        return mono_dst_[dstIdx];
    }

    /**
     * Returns the final modulated value for a poly destination for a specific voice, in plain units.
     * Call this after process() to retrieve the modulated parameter value.
     */
    [[nodiscard]] float getPolyModValue(uint16_t dstIdx, uint16_t voice) const {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        ASSERT(voice < config_.num_voices, "Voice index out of bounds");
        return poly_dst_buf_[static_cast<size_t>(voice) * poly_dst_stride_ + dstIdx];
    }

    /**
     * Get a handle for direct access to a mono destination's modulated value.
     * @param dstIdx The destination index
     * @return Handle pointing directly to the value in mono_dst_
     */
    [[nodiscard]] ModParamHandle getModHandle(uint16_t dstIdx) {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        return ModParamHandle{&mono_dst_[dstIdx]};
    }

    /**
     * Get a handle for direct access to a poly destination's modulated value for a specific voice.
     * @param dstIdx The destination index
     * @param voice The voice index
     * @return Handle pointing directly to the value for this (destination, voice) pair
     */
    [[nodiscard]] ModParamHandle getModHandle(uint16_t dstIdx, uint16_t voice) {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        ASSERT(voice < config_.num_voices, "Voice index out of bounds");
        return ModParamHandle{&poly_dst_buf_[static_cast<size_t>(voice) * poly_dst_stride_ + dstIdx]};
    }

    /**
     * Load all param values as normalized into base destination values.
     * Assumes param index == destination index (1:1 bijection).
     * Call once per block before process().
     */
    void loadParamBaseValues(const applause::ParamsExtension& params);

    /**
     * Processes modulation. Should be called once per block, before parameters are read and used by DSP components.
     */
    void process();

private:
    // Depth accessors for ModConnection (avoids dangling pointer issues)
    [[nodiscard]] float getDepthBase(uint16_t slot) const { return program_.depth_base_[slot]; }
    void setDepthBase(uint16_t slot, float value) { program_.depth_base_[slot] = value; }

    /**
     * Allocates a depth slot, reusing tombstoned slots when available.
     */
    uint16_t allocateDepthSlot(float initial_depth);

    /**
     * Rebuilds the modulation program object. Called whenever the user changes the modulation matrix state.
     */
    void recompileProgram();

    const Config config_;
    const uint32_t poly_src_stride_;  // = max_sources
    const uint32_t poly_dst_stride_;  // = max_destinations
    const uint32_t poly_depth_stride_;  // = max_connections

    ModProgram program_;

    int src_count_ = 0;
    int dst_count_ = 0;

    std::vector<uint16_t> active_voices_;

    std::unordered_map<std::string, uint16_t> src_lookup_;
    std::unordered_map<std::string, uint16_t> dst_lookup_;

    std::vector<ModSource> src_registry_;
    std::vector<ModDestination> dst_registry_;
    std::vector<applause::ValueScaleInfo> dst_scale_info_;
    std::vector<uint16_t> poly_dst_indices_;  // indices of poly destinations only; small optimization

    // Source values (written by modulators before processBlock)
    std::vector<float> mono_src_buf_;
    std::vector<float> poly_src_buf_;

    // Base destination values (unmodulated knob values, optional)
    std::vector<float> base_mono_dst_;
    std::vector<float> base_poly_dst_;

    std::vector<float> mono_depth_buf_;
    std::vector<float> poly_depth_buf_;

    std::vector<float> mono_dst_;
    std::vector<float> poly_dst_buf_;

    std::vector<ModConnection> connections_;

    friend class ModProgram;
    friend struct ModConnection;
};

}  // namespace applause
