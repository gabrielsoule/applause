#pragma once

#include <algorithm>
#include <array>
#include <applause/extensions/ParamsExtension.h>
#include <applause/util/DebugHelpers.h>
#include <applause/util/SampleType.h>
#include <applause/util/ValueScaling.h>
#include <applause/util/thirdparty/rocket.hpp>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace applause {

enum class ModSrcType : uint8_t { Mono, Poly, Both };

enum class ModSrcMode : uint8_t { Mono, Poly };

enum class ModDstMode : uint8_t { Mono, Poly };

template <typename T>
concept ModSignal = Sample<T> && std::same_as<scalar_t<T>, float>;

class ModMatrixControl;

template <ModSignal Signal>
class ModMatrix;

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
    ModMatrixControl* matrix = nullptr;  ///< Set by registerSource(); identifies the owning matrix.
};

struct ModDestination {
    std::string name;
    uint16_t index;
    ModDstMode mode;
    ModMatrixControl* matrix = nullptr;  ///< Set by registerDestination(); identifies the owning matrix.
};

/**
 * Unified modulation connection structure.
 * Holds a pointer to its parent ModMatrix for safe depth access (avoids dangling pointers
 * that could occur if we stored raw pointers into reallocating vectors).
 */
struct ModConnection {
    static constexpr uint8_t kFlagDepthMod = 1u << 0;  ///< Connection modulates another connection's depth
    static constexpr uint8_t kFlagBipolar = 1u << 1;  ///< Output is centered at 0 (bidirectional mapping)

    ModMatrixControl* matrix_ = nullptr;  ///< Parent matrix (required for depth/recompile access)
    uint16_t src_idx = 0;  ///< Source index
    uint16_t dst_idx = 0;  ///< Destination index (param conn) OR target depth slot (depth mod)
    uint16_t depth_slot = 0;  ///< Slot index where this connection's depth is stored
    uint8_t flags = 0;  ///< Packed flags; see kFlag* constants above

    [[nodiscard]] bool isDepthMod() const { return flags & kFlagDepthMod; }
    [[nodiscard]] bool isBipolar() const { return flags & kFlagBipolar; }

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
    static constexpr uint8_t kFlagDepthMod = 1u << 0;  ///< Connection modulates another connection's depth
    static constexpr uint8_t kFlagSrcBipolar = 1u << 1;  ///< Source's native output range is [-1, +1]
    static constexpr uint8_t kFlagBipolar = 1u << 2;  ///< Output is centered at 0 (bidirectional mapping)

    uint16_t src;  ///< Source index
    uint16_t target;  ///< Destination index (param conn) OR target depth slot (depth mod)
    uint16_t depth_slot;  ///< Slot index where this connection's depth is stored
    uint8_t flags;  ///< Packed flags; see kFlag* constants above

    [[nodiscard]] bool isDepthMod() const { return flags & kFlagDepthMod; }
    [[nodiscard]] bool isSourceBipolar() const { return flags & kFlagSrcBipolar; }
    [[nodiscard]] bool isBipolar() const { return flags & kFlagBipolar; }
};

/**
 * Lightweight handle for audio-thread access to a modulated parameter value.
 * Use getModHandle() to obtain. The pointer is pre-computed, so getValue()
 * is a simple dereference with no arithmetic.
 */
template <ModSignal Value>
struct ModParamHandle {
    Value* value_ = nullptr;
    [[nodiscard]] Value getValue() const noexcept { return *value_; }
};

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
    std::vector<ModConnectionHandle> mm_connections;  // mono src -> mono dst
    std::vector<ModConnectionHandle> mp_connections;  // mono src -> poly dst
    std::vector<ModConnectionHandle> pm_connections;  // poly src -> mono dst (NYI)
    std::vector<ModConnectionHandle> pp_connections;  // poly src -> poly dst

    std::vector<float> depth_base_;
    std::vector<uint8_t> depth_active_;

    std::vector<ModConnectionHandle> depth_connections_mono_;
    std::vector<ModConnectionHandle> depth_connections_poly_;

public:
    explicit ModProgram(uint16_t max_connections) {
        mm_connections.reserve(max_connections);
        mp_connections.reserve(max_connections);
        pm_connections.reserve(max_connections);
        pp_connections.reserve(max_connections);
        depth_base_.reserve(max_connections);
        depth_active_.reserve(max_connections);
        depth_connections_mono_.reserve(max_connections);
        depth_connections_poly_.reserve(max_connections);
    }

    friend class ModMatrixControl;

    template <ModSignal>
    friend class ModMatrix;
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
 * The modulation system can be templated with respect to scalar modulation signals or "batched" SIMD signals.
 * The templated material is split into a separate class to minimize template contagion elsewhere in the codebase.
 *
 * Deeper modulation graphs would necessitate development of a signal digraph processing system, a graph compiler,
 * loop detection & resolution, et cetera. We omit this for now, as deeper recursive modulation (i.e. modulation of a
 * connection that modulates a connection that modulates a parameter) is an edge case not required within most synthesis
 * applications.
 *
 * This might be nice in the future, and a "ModMatrix 2.0" is on the distant roadmap.
 *
 */
class ModMatrixControl {
public:
    using Mask = std::uint64_t;

    struct Config {
        uint16_t num_voices;  // A SIMD batch is one matrix voice.
        uint16_t max_sources;
        uint16_t max_destinations;
        uint16_t max_connections;
    };

protected:
    explicit ModMatrixControl(Config config, Mask valid_mask) :
        config_(config),
        valid_mask_(valid_mask),
        program_(config.max_connections),
        active_masks_(config.num_voices, 0),
        src_registry_(config.max_sources),
        dst_registry_(config.max_destinations),
        dst_scale_info_(config.max_destinations),
        mono_src_buf_(config.max_sources, 0.0f),
        base_dst_(config.max_destinations, 0.0f),
        mono_depth_buf_(config.max_connections, 0.0f),
        mono_dst_(config.max_destinations, 0.0f) {
        active_voices_.reserve(config.num_voices);
    }

    virtual ~ModMatrixControl() = default;

    ModMatrixControl() = delete;
    ModMatrixControl(const ModMatrixControl&) = delete;
    ModMatrixControl& operator=(const ModMatrixControl&) = delete;
    ModMatrixControl(ModMatrixControl&&) = delete;
    ModMatrixControl& operator=(ModMatrixControl&&) = delete;

public:
    /**
     * Registers a modulation source. Write its values with setMonoSourceValue() or setPolySourceValue().
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
     * @return copy of the created/updated connection
     */
    ModConnection addConnection(ModSource src, ModDestination dst, float depth = 1.0f,
                                std::optional<bool> bipolar_mapping = std::nullopt);

    /**
     * Removes a modulation connection between a source and destination. Note that any depth-mod connections that
     * target this connection's depth as a destination will also be removed.
     * @param srcIdx the source index
     * @param dstIdx the destination index
     * @return true if a connection was found and removed, false otherwise
     */
    bool removeConnection(uint16_t srcIdx, uint16_t dstIdx);

    /**
     * Removes a modulation connection. Same cascade semantics as the (srcIdx, dstIdx) overload.
     */
    bool removeConnection(const ModConnection& connection);

    /**
     * Reassigns the source of an existing connection in place. Preserves the connection's depth value
     * and its bipolar flag. If any depth-mod connections target this connection's depth as a destination,
     * they will be preserved and redirected appropriately.
     *
     * Works for both parameter connections and depth mods.
     *
     * If a connection already exists with the same (newSrc, dst) pair, the connection is merged
     * into the preexisting: the preexisting adopts this connection's depth and flags, and this
     * connection is **removed**.
     *
     * Depth mods attached to the moving connection are transferred onto the preexisting, except where the
     * preexisting already has a depth mod from the same source. In that case, the preexisting's existing depth
     * mod wins and the moving one is dropped.
     *
     * @return a copy of the resulting connection (the in-place one, or the preexisting if merged)
     */
    ModConnection reassignSource(const ModConnection& conn, ModSource newSrc);

    /**
     * Reassigns the destination of an existing parameter connection in place. Preserves the
     * connection's depth value and its bipolar flag. Depth values are unit-free (scaling is
     * applied to the final destination value only), so no rescaling is needed. If any depth-mod
     * connections target this connection's depth as a destination, they will be preserved and
     * redirected appropriately.
     *
     * Parameter connections only; asserts if called on a depth mod.
     *
     * If a connection already exists with the same (src, newDst) pair, the connection is merged
     * into the preexisting: the preexisting adopts this connection's depth and flags, and this
     * connection is **removed**.
     *
     * Depth mods attached to the moving connection are transferred onto the preexisting, except where the
     * preexisting already has a depth mod from the same source. In that case, the preexisting's existing depth
     * mod wins and the moving one is dropped.
     *
     * @return a copy of the resulting connection (the in-place one, or the preexisting if merged)
     */
    ModConnection reassignDestination(const ModConnection& conn, ModDestination newDst);

    [[nodiscard]] const std::vector<ModConnection>& getConnections() const { return connections_; }

    /**
     * Returns true if any connection targets the given destination. This function disregards second-order connections,
     * i.e. when a source is modulating the depth of another extant connection.
     */
    [[nodiscard]] bool dstIsConnected(uint16_t dstIdx) const;

    /**
     * Returns true if any connection originates from the given source. This may include second-order depth-modulation
     * connections.
     */
    [[nodiscard]] bool srcIsConnected(uint16_t srcIdx) const;

    /**
     * Fires whenever the connection graph is changed, e.g. after adding/removing a connection.
     */
    mutable rocket::signal<void()> on_connections_changed;

    /**
     * Finds a first-order (non-depth-mod) connection by source and destination index.
     */
    [[nodiscard]] std::optional<ModConnection> findConnection(uint16_t srcIdx, uint16_t dstIdx);

    /**
     * Finds a first-order (non-depth-mod) connection by its depth slot.
     */
    [[nodiscard]] std::optional<ModConnection> findConnection(uint16_t depthSlot);

    /**
     * Finds a depth-mod connection by source index and target depth slot.
     * @param srcIdx the modulation source index
     * @param targetDepthSlot the depth_slot of the connection being modulated
     */
    [[nodiscard]] std::optional<ModConnection> findDepthMod(uint16_t srcIdx, uint16_t targetDepthSlot);

    [[nodiscard]] ModSource* findSource(const std::string& name);

    [[nodiscard]] const ModSource* findSource(const std::string& name) const;

    [[nodiscard]] ModDestination* findDestination(const std::string& name);

    [[nodiscard]] const ModDestination* findDestination(const std::string& name) const;

    /**
     * Adds a modulation connection that modulates the depth of an existing connection. If a depth modulation
     * already exists with the same source targeting the same connection, it will be updated in place rather than
     * duplicated.
     *
     * @param src the modulation source
     * @param target_conn the connection whose depth should be modulated
     * @param depth the modulation depth for this depth-modulation connection
     * @param bipolar_mapping whether the output should be centered at 0 (bidirectional).
     *        If not specified, defaults to the source's bipolar flag.
     * @return copy of the created/updated depth modulation connection
     */
    ModConnection addDepthModulation(ModSource src, const ModConnection& target_conn, float depth = 1.0f,
                                     std::optional<bool> bipolar_mapping = std::nullopt);

    /** Activates every lane of a matrix voice. SIMD callers with partial batches use setActiveMask(). */
    void notifyVoiceOn(uint16_t voice_index) noexcept;

    /** Deactivates every lane of a matrix voice. */
    void notifyVoiceOff(uint16_t voice_index) noexcept;

    /** Sets the active-lane mask for one matrix voice. Zero deactivates the voice. */
    void setActiveMask(uint16_t voice_index, Mask mask) noexcept;

    /**
     * Copies as many active destination values as fit in output, in plain units.
     * Returns the total available count. Pass an empty span to query the count.
     * Poly values are ordered by active matrix voice, then SIMD lane.
     */
    [[nodiscard]] virtual size_t copyActiveDestinationValues(uint16_t dstIdx,
                                                             std::span<float> output) const noexcept = 0;

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
     * Returns the final modulated value for a mono destination in plain units.
     * Call this after process() to retrieve the modulated parameter value.
     */
    [[nodiscard]] float getModValue(uint16_t dstIdx) const {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        return mono_dst_[dstIdx];
    }

    /**
     * Returns the cumulative modulation offset range `{min_offset, max_offset}` for a destination, summed
     * over all parameter-modulating connections that target it. Result is in normalized in [0,1] with
     * respect to the underlying destination's normalized range. Use this to draw "modulation depth arcs" on
     * knobs or sliders or whatever.
     */
    [[nodiscard]] std::pair<float, float> getModOffsetRange(uint16_t dstIdx) const;

    /**
     * Get a handle for direct access to a mono destination's modulated value.
     * @param dstIdx The destination index
     * @return Handle pointing directly to the value in mono_dst_
     */
    [[nodiscard]] ModParamHandle<float> getModHandle(uint16_t dstIdx) {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        return {&mono_dst_[dstIdx]};
    }

    /**
     * Load all param values as normalized into base destination values.
     * Assumes param index == destination index (1:1 bijection).
     * Call once per block in your process function, before you do processing
     * @return true if any base value changed.
     */
    bool loadParamBaseValues(const applause::ParamsExtension& params) noexcept;

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
    const Mask valid_mask_;

    ModProgram program_;

    int src_count_ = 0;
    int dst_count_ = 0;
    uint16_t param_dst_count_ = 0;

    std::vector<uint16_t> active_voices_;
    std::vector<Mask> active_masks_;

    std::unordered_map<std::string, uint16_t> src_lookup_;
    std::unordered_map<std::string, uint16_t> dst_lookup_;

    std::vector<ModSource> src_registry_;
    std::vector<ModDestination> dst_registry_;
    std::vector<applause::ValueScaleInfo> dst_scale_info_;
    std::vector<uint16_t> mono_dst_indices_;
    std::vector<uint16_t> poly_dst_indices_;

    // Source values (written by modulators before processBlock)
    std::vector<float> mono_src_buf_;

    // Base destination values (unmodulated knob values, optional)
    std::vector<float> base_dst_;

    std::vector<float> mono_depth_buf_;

    std::vector<float> mono_dst_;

    std::vector<ModConnection> connections_;

    friend struct ModConnection;

    template <ModSignal>
    friend class ModMatrix;
};

template <ModSignal Signal>
class ModMatrix final : public ModMatrixControl {
    static_assert(sample_width_v<Signal> <= 64);

    static constexpr Mask valid_mask = [] {
        if constexpr (sample_width_v<Signal> == 64)
            return ~Mask{0};
        else
            return (Mask{1} << sample_width_v<Signal>) - 1;
    }();

public:
    using Config = ModMatrixControl::Config;

    explicit ModMatrix(Config config) :
        ModMatrixControl(config, valid_mask),
        poly_src_stride_(config.max_sources),
        poly_dst_stride_(config.max_destinations),
        poly_depth_stride_(config.max_connections),
        poly_src_buf_(static_cast<size_t>(config.num_voices) * config.max_sources, set1<Signal>(0.0f)),
        poly_depth_buf_(static_cast<size_t>(config.num_voices) * config.max_connections, set1<Signal>(0.0f)),
        poly_dst_buf_(static_cast<size_t>(config.num_voices) * config.max_destinations, set1<Signal>(0.0f)) {}

    ModMatrix() = delete;
    ModMatrix(const ModMatrix&) = delete;
    ModMatrix& operator=(const ModMatrix&) = delete;
    ModMatrix(ModMatrix&&) = delete;
    ModMatrix& operator=(ModMatrix&&) = delete;

    void setPolySourceValue(uint16_t srcIdx, uint16_t voice, Signal value) {
        ASSERT(srcIdx < src_count_, "Source index out of bounds");
        ASSERT(voice < config_.num_voices, "Voice index out of bounds");
        poly_src_buf_[static_cast<size_t>(voice) * poly_src_stride_ + srcIdx] = value;
    }

    void setActiveMask(uint16_t voice, Mask mask) noexcept
        requires Scalar<Signal>
    {
        ModMatrixControl::setActiveMask(voice, mask);
    }

    void setActiveMask(uint16_t voice, const mask_t<Signal>& mask) noexcept
        requires SimdBatch<Signal>
    {
        ModMatrixControl::setActiveMask(voice, mask.mask());
    }

    [[nodiscard]] Signal getPolyModValue(uint16_t dstIdx, uint16_t voice) const {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        ASSERT(voice < config_.num_voices, "Voice index out of bounds");
        return poly_dst_buf_[static_cast<size_t>(voice) * poly_dst_stride_ + dstIdx];
    }

    using ModMatrixControl::getModHandle;

    [[nodiscard]] ModParamHandle<Signal> getModHandle(uint16_t dstIdx, uint16_t voice) {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        ASSERT(voice < config_.num_voices, "Voice index out of bounds");
        return {&poly_dst_buf_[static_cast<size_t>(voice) * poly_dst_stride_ + dstIdx]};
    }

    [[nodiscard]] size_t copyActiveDestinationValues(uint16_t dstIdx,
                                                     std::span<float> output) const noexcept override;

    /** Processes modulation at the cadence selected by the owning DSP. */
    void process();

private:
    const uint32_t poly_src_stride_;
    const uint32_t poly_dst_stride_;
    const uint32_t poly_depth_stride_;

    std::vector<Signal> poly_src_buf_;
    std::vector<Signal> poly_depth_buf_;
    std::vector<Signal> poly_dst_buf_;
};

template <ModSignal Value>
static inline Value applyConnectionPolarity(Value value, bool source_bipolar, bool bipolar_mapping) {
    if (source_bipolar) value = (value + set1<Value>(1.0f)) * set1<Value>(0.5f);
    if (bipolar_mapping) value -= set1<Value>(0.5f);
    return value;
}

inline ModSource& ModMatrixControl::registerSource(const std::string& string_id, ModSrcType type, bool bipolar,
                                                   ModSrcMode defaultMode) {
    ASSERT(src_count_ < config_.max_sources, "max_sources exceeded");
    ASSERT(!src_lookup_.contains(string_id), "Source name already registered");

    const auto idx = static_cast<uint16_t>(src_count_++);
    src_lookup_.emplace(string_id, idx);

    auto& source = src_registry_[idx];
    source.name = string_id;
    source.index = idx;
    source.type = type;
    source.mode = type == ModSrcType::Both ? defaultMode
        : type == ModSrcType::Poly         ? ModSrcMode::Poly
                                           : ModSrcMode::Mono;
    source.bipolar = bipolar;
    source.matrix = this;
    return source;
}

inline void ModMatrixControl::setSourceMode(uint16_t srcIdx, ModSrcMode mode) {
    ASSERT(srcIdx < src_count_, "Source index out of bounds");
    ASSERT(src_registry_[srcIdx].type == ModSrcType::Both,
           "setSourceMode only valid for sources with ModSrcType::Both");
    src_registry_[srcIdx].mode = mode;
    recompileProgram();
}

inline ModDestination& ModMatrixControl::registerDestination(const std::string& string_id, ModDstMode mode,
                                                             ValueScaleInfo scale_info) {
    ASSERT(dst_count_ < config_.max_destinations, "max_destinations exceeded");
    ASSERT(!dst_lookup_.contains(string_id), "Destination name already registered");

    const auto idx = static_cast<uint16_t>(dst_count_++);
    dst_lookup_.emplace(string_id, idx);

    auto& destination = dst_registry_[idx];
    destination.name = string_id;
    destination.index = idx;
    destination.mode = mode;
    destination.matrix = this;
    dst_scale_info_[idx] = scale_info;
    (mode == ModDstMode::Mono ? mono_dst_indices_ : poly_dst_indices_).push_back(idx);
    return destination;
}

inline void ModMatrixControl::registerFromParamsExtension(const ParamsExtension& params_extension) {
    ASSERT(dst_count_ == 0, "Register parameters before other destinations");
    const auto* scales = params_extension.getScaleInfoArray();
    const auto& params = params_extension.getAllParameters();

    for (uint32_t i = 0; i < params.size(); ++i) {
        const auto& param = params[i];
        registerDestination(param.stringId, param.polyphonic ? ModDstMode::Poly : ModDstMode::Mono, scales[i]);
    }
    param_dst_count_ = static_cast<uint16_t>(params.size());
    loadParamBaseValues(params_extension);
}

inline ModConnection ModMatrixControl::addConnection(ModSource src, ModDestination dst, float depth,
                                                     std::optional<bool> bipolar_mapping) {
    ASSERT(src.matrix == this && dst.matrix == this, "Source and destination must belong to this matrix");
    ASSERT(src.index < src_count_, "Source index out of bounds");
    ASSERT(dst.index < dst_count_, "Destination index out of bounds");

    bool mapping;
    if (bipolar_mapping.has_value()) {
        mapping = *bipolar_mapping;
    } else if (src_registry_[src.index].bipolar) {
        mapping = true;
    } else {
        constexpr float center_lo = 1.0f / 3.0f;
        constexpr float center_hi = 2.0f / 3.0f;
        const float base = base_dst_[dst.index];
        if (base < center_lo) {
            mapping = false;
            depth = std::fabs(depth);
        } else if (base > center_hi) {
            mapping = false;
            depth = -std::fabs(depth);
        } else {
            mapping = true;
        }
    }

    for (auto& existing : connections_) {
        if (!existing.isDepthMod() && existing.src_idx == src.index && existing.dst_idx == dst.index) {
            program_.depth_base_[existing.depth_slot] = depth;
            existing.flags =
                mapping ? existing.flags | ModConnection::kFlagBipolar : existing.flags & ~ModConnection::kFlagBipolar;
            recompileProgram();
            return existing;
        }
    }

    ModConnection connection{};
    connection.matrix_ = this;
    connection.src_idx = src.index;
    connection.dst_idx = dst.index;
    connection.flags = mapping ? ModConnection::kFlagBipolar : 0;
    connection.depth_slot = allocateDepthSlot(depth);
    connections_.push_back(connection);
    recompileProgram();
    return connections_.back();
}

inline bool ModMatrixControl::removeConnection(uint16_t srcIdx, uint16_t dstIdx) {
    if (auto connection = findConnection(srcIdx, dstIdx)) return removeConnection(*connection);
    return false;
}

inline bool ModMatrixControl::removeConnection(const ModConnection& connection) {
    ASSERT(connection.matrix_ == this, "Connection must belong to this matrix");
    const auto it = std::ranges::find_if(
        connections_, [&](const auto& candidate) { return candidate.depth_slot == connection.depth_slot; });
    if (it == connections_.end()) return false;

    const uint16_t freed_slot = it->depth_slot;
    const bool was_parameter_connection = !it->isDepthMod();
    program_.depth_active_[freed_slot] = 0;
    connections_.erase(it);

    if (was_parameter_connection) {
        std::erase_if(connections_, [this, freed_slot](const auto& candidate) {
            if (!candidate.isDepthMod() || candidate.dst_idx != freed_slot) return false;
            program_.depth_active_[candidate.depth_slot] = 0;
            return true;
        });
    }
    recompileProgram();
    return true;
}

inline std::optional<ModConnection> ModMatrixControl::findConnection(uint16_t srcIdx, uint16_t dstIdx) {
    for (const auto& connection : connections_) {
        if (!connection.isDepthMod() && connection.src_idx == srcIdx && connection.dst_idx == dstIdx) return connection;
    }
    return std::nullopt;
}

inline std::optional<ModConnection> ModMatrixControl::findConnection(uint16_t depthSlot) {
    for (const auto& connection : connections_) {
        if (!connection.isDepthMod() && connection.depth_slot == depthSlot) return connection;
    }
    return std::nullopt;
}

inline std::optional<ModConnection> ModMatrixControl::findDepthMod(uint16_t srcIdx, uint16_t targetDepthSlot) {
    for (const auto& connection : connections_) {
        if (connection.isDepthMod() && connection.src_idx == srcIdx && connection.dst_idx == targetDepthSlot)
            return connection;
    }
    return std::nullopt;
}

inline ModSource* ModMatrixControl::findSource(const std::string& name) {
    const auto it = src_lookup_.find(name);
    return it == src_lookup_.end() ? nullptr : &src_registry_[it->second];
}

inline const ModSource* ModMatrixControl::findSource(const std::string& name) const {
    const auto it = src_lookup_.find(name);
    return it == src_lookup_.end() ? nullptr : &src_registry_[it->second];
}

inline ModDestination* ModMatrixControl::findDestination(const std::string& name) {
    const auto it = dst_lookup_.find(name);
    return it == dst_lookup_.end() ? nullptr : &dst_registry_[it->second];
}

inline const ModDestination* ModMatrixControl::findDestination(const std::string& name) const {
    const auto it = dst_lookup_.find(name);
    return it == dst_lookup_.end() ? nullptr : &dst_registry_[it->second];
}

inline std::pair<float, float> ModMatrixControl::getModOffsetRange(uint16_t dstIdx) const {
    ASSERT(dstIdx < dst_count_, "Destination index out of bounds");

    float minimum = 0.0f;
    float maximum = 0.0f;
    for (const auto& connection : connections_) {
        if (connection.isDepthMod() || connection.dst_idx != dstIdx) continue;
        const float depth = program_.depth_base_[connection.depth_slot];
        const float magnitude = std::fabs(depth);
        if (connection.isBipolar()) {
            minimum -= magnitude * 0.5f;
            maximum += magnitude * 0.5f;
        } else if (depth >= 0.0f) {
            maximum += depth;
        } else {
            minimum += depth;
        }
    }
    return {minimum, maximum};
}

inline ModConnection ModMatrixControl::addDepthModulation(ModSource src, const ModConnection& target_conn, float depth,
                                                          std::optional<bool> bipolar_mapping) {
    ASSERT(src.matrix == this && target_conn.matrix_ == this, "Connections must belong to this matrix");
    ASSERT(src.index < src_count_, "Source index out of bounds");
    ASSERT(target_conn.depth_slot < program_.depth_base_.size(), "Invalid target connection");
    ASSERT(!target_conn.isDepthMod(), "Depth modulation is limited to one level");

    const bool mapping = bipolar_mapping.value_or(src_registry_[src.index].bipolar);
    const uint16_t target_slot = target_conn.depth_slot;
    for (auto& existing : connections_) {
        if (existing.isDepthMod() && existing.src_idx == src.index && existing.dst_idx == target_slot) {
            program_.depth_base_[existing.depth_slot] = depth;
            existing.flags =
                mapping ? existing.flags | ModConnection::kFlagBipolar : existing.flags & ~ModConnection::kFlagBipolar;
            recompileProgram();
            return existing;
        }
    }

    ModConnection connection{};
    connection.matrix_ = this;
    connection.src_idx = src.index;
    connection.dst_idx = target_slot;
    connection.flags = ModConnection::kFlagDepthMod | (mapping ? ModConnection::kFlagBipolar : 0);
    connection.depth_slot = allocateDepthSlot(depth);
    connections_.push_back(connection);
    recompileProgram();
    return connections_.back();
}

inline ModConnection ModMatrixControl::reassignSource(const ModConnection& connection, ModSource newSource) {
    ASSERT(connection.matrix_ == this && newSource.matrix == this, "Objects must belong to this matrix");
    ASSERT(newSource.index < src_count_, "Source index out of bounds");
    auto it = std::ranges::find_if(
        connections_, [&](const auto& candidate) { return candidate.depth_slot == connection.depth_slot; });
    ASSERT(it != connections_.end(), "Connection not found");
    if (it->src_idx == newSource.index) return *it;

    for (auto& existing : connections_) {
        if (existing.depth_slot == it->depth_slot) continue;
        if (existing.isDepthMod() != it->isDepthMod() || existing.src_idx != newSource.index ||
            existing.dst_idx != it->dst_idx)
            continue;

        program_.depth_base_[existing.depth_slot] = program_.depth_base_[it->depth_slot];
        existing.flags = it->flags;
        const uint16_t old_slot = it->depth_slot;
        const uint16_t new_slot = existing.depth_slot;
        for (auto& depth_connection : connections_) {
            if (!depth_connection.isDepthMod() || depth_connection.dst_idx != old_slot) continue;
            const bool conflict = std::ranges::any_of(connections_, [&](const auto& candidate) {
                return candidate.isDepthMod() && candidate.src_idx == depth_connection.src_idx &&
                    candidate.dst_idx == new_slot;
            });
            if (!conflict) depth_connection.dst_idx = new_slot;
        }
        const auto result = existing;
        removeConnection(*it);
        return result;
    }

    it->src_idx = newSource.index;
    recompileProgram();
    return *it;
}

inline ModConnection ModMatrixControl::reassignDestination(const ModConnection& connection,
                                                           ModDestination newDestination) {
    ASSERT(connection.matrix_ == this && newDestination.matrix == this, "Objects must belong to this matrix");
    ASSERT(!connection.isDepthMod(), "Cannot reassign a depth modulation destination");
    ASSERT(newDestination.index < dst_count_, "Destination index out of bounds");
    auto it = std::ranges::find_if(
        connections_, [&](const auto& candidate) { return candidate.depth_slot == connection.depth_slot; });
    ASSERT(it != connections_.end(), "Connection not found");
    if (it->dst_idx == newDestination.index) return *it;

    for (auto& existing : connections_) {
        if (existing.depth_slot == it->depth_slot) continue;
        if (existing.isDepthMod() || existing.src_idx != it->src_idx || existing.dst_idx != newDestination.index)
            continue;

        program_.depth_base_[existing.depth_slot] = program_.depth_base_[it->depth_slot];
        existing.flags = it->flags;
        const uint16_t old_slot = it->depth_slot;
        const uint16_t new_slot = existing.depth_slot;
        for (auto& depth_connection : connections_) {
            if (!depth_connection.isDepthMod() || depth_connection.dst_idx != old_slot) continue;
            const bool conflict = std::ranges::any_of(connections_, [&](const auto& candidate) {
                return candidate.isDepthMod() && candidate.src_idx == depth_connection.src_idx &&
                    candidate.dst_idx == new_slot;
            });
            if (!conflict) depth_connection.dst_idx = new_slot;
        }
        const auto result = existing;
        removeConnection(*it);
        return result;
    }

    it->dst_idx = newDestination.index;
    recompileProgram();
    return *it;
}

inline void ModMatrixControl::notifyVoiceOn(uint16_t voice_index) noexcept {
    setActiveMask(voice_index, valid_mask_);
}

inline void ModMatrixControl::notifyVoiceOff(uint16_t voice_index) noexcept {
    setActiveMask(voice_index, 0);
}

inline void ModMatrixControl::setActiveMask(uint16_t voice_index, Mask mask) noexcept {
    ASSERT(voice_index < config_.num_voices, "Voice index out of bounds");
    ASSERT((mask & ~valid_mask_) == 0, "Mask contains invalid lanes");
    mask &= valid_mask_;

    auto& current = active_masks_[voice_index];
    if (current == mask) return;

    const bool was_active = current != 0;
    current = mask;
    if (!was_active && mask != 0)
        active_voices_.push_back(voice_index);
    else if (was_active && mask == 0)
        std::erase(active_voices_, voice_index);
}

inline void ModMatrixControl::setBaseValue(uint16_t dstIdx, float plain_value) {
    ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
    const auto& scale = dst_scale_info_[dstIdx];
    base_dst_[dstIdx] = scale.scaling.toNormalized(plain_value, scale.min, scale.max);
}

inline bool ModMatrixControl::loadParamBaseValues(const ParamsExtension& params) noexcept {
    const auto param_count = params.getParamCount();
    ASSERT(param_count == param_dst_count_, "Parameter count changed after destination registration");
    if (param_count != param_dst_count_) return false;

    bool changed = false;
    const auto* values = params.getValuesArray();
    for (uint16_t i = 0; i < param_dst_count_; ++i) {
        const auto& scale = dst_scale_info_[i];
        const float plain = values[i].load(std::memory_order_relaxed);
        const float normalized = scale.scaling.toNormalized(plain, scale.min, scale.max);
        changed |= normalized != base_dst_[i];
        base_dst_[i] = normalized;
    }
    return changed;
}

template <ModSignal Signal>
void ModMatrix<Signal>::process() {
    for (uint16_t destination : mono_dst_indices_) mono_dst_[destination] = base_dst_[destination];

    for (uint16_t voice : active_voices_) {
        const size_t destination_offset = static_cast<size_t>(voice) * poly_dst_stride_;
        for (uint16_t destination : poly_dst_indices_)
            poly_dst_buf_[destination_offset + destination] = set1<Signal>(base_dst_[destination]);
    }

    for (size_t slot = 0; slot < program_.depth_base_.size(); ++slot)
        mono_depth_buf_[slot] = program_.depth_active_[slot] ? program_.depth_base_[slot] : 0.0f;

    for (const auto& connection : program_.depth_connections_mono_) {
        const float source = applyConnectionPolarity(mono_src_buf_[connection.src], connection.isSourceBipolar(),
                                                     connection.isBipolar());
        mono_depth_buf_[connection.target] += source * program_.depth_base_[connection.depth_slot];
    }

    for (uint16_t voice : active_voices_) {
        const size_t depth_offset = static_cast<size_t>(voice) * poly_depth_stride_;
        for (size_t slot = 0; slot < program_.depth_base_.size(); ++slot)
            poly_depth_buf_[depth_offset + slot] = set1<Signal>(mono_depth_buf_[slot]);
    }

    for (uint16_t voice : active_voices_) {
        const size_t source_offset = static_cast<size_t>(voice) * poly_src_stride_;
        const size_t depth_offset = static_cast<size_t>(voice) * poly_depth_stride_;
        for (const auto& connection : program_.depth_connections_poly_) {
            const Signal source = applyConnectionPolarity(poly_src_buf_[source_offset + connection.src],
                                                          connection.isSourceBipolar(), connection.isBipolar());
            const Signal depth = set1<Signal>(program_.depth_base_[connection.depth_slot]);
            poly_depth_buf_[depth_offset + connection.target] += source * depth;
        }
    }

    for (const auto& connection : program_.mm_connections) {
        const float source = applyConnectionPolarity(mono_src_buf_[connection.src], connection.isSourceBipolar(),
                                                     connection.isBipolar());
        mono_dst_[connection.target] += source * mono_depth_buf_[connection.depth_slot];
    }

    for (uint16_t voice : active_voices_) {
        const size_t depth_offset = static_cast<size_t>(voice) * poly_depth_stride_;
        const size_t destination_offset = static_cast<size_t>(voice) * poly_dst_stride_;
        for (const auto& connection : program_.mp_connections) {
            const float source = applyConnectionPolarity(mono_src_buf_[connection.src], connection.isSourceBipolar(),
                                                         connection.isBipolar());
            poly_dst_buf_[destination_offset + connection.target] +=
                set1<Signal>(source) * poly_depth_buf_[depth_offset + connection.depth_slot];
        }
    }

    for (uint16_t voice : active_voices_) {
        const size_t source_offset = static_cast<size_t>(voice) * poly_src_stride_;
        const size_t depth_offset = static_cast<size_t>(voice) * poly_depth_stride_;
        const size_t destination_offset = static_cast<size_t>(voice) * poly_dst_stride_;
        for (const auto& connection : program_.pp_connections) {
            const Signal source = applyConnectionPolarity(poly_src_buf_[source_offset + connection.src],
                                                          connection.isSourceBipolar(), connection.isBipolar());
            poly_dst_buf_[destination_offset + connection.target] +=
                source * poly_depth_buf_[depth_offset + connection.depth_slot];
        }
    }

    // Poly-to-mono routes still need a lane reduction algorithm...

    for (uint16_t destination : mono_dst_indices_) {
        const auto& scale = dst_scale_info_[destination];
        const float normalized = std::clamp(mono_dst_[destination], 0.0f, 1.0f);
        mono_dst_[destination] = scale.scaling.fromNormalized(normalized, scale.min, scale.max);
    }

    const Signal zero = set1<Signal>(0.0f);
    const Signal one = set1<Signal>(1.0f);
    for (uint16_t voice : active_voices_) {
        const size_t destination_offset = static_cast<size_t>(voice) * poly_dst_stride_;
        for (uint16_t destination : poly_dst_indices_) {
            const auto& scale = dst_scale_info_[destination];
            const Signal normalized =
                applause::min(applause::max(poly_dst_buf_[destination_offset + destination], zero), one);
            poly_dst_buf_[destination_offset + destination] =
                scale.scaling.fromNormalized(normalized, scale.min, scale.max);
        }
    }
}

template <ModSignal Signal>
size_t ModMatrix<Signal>::copyActiveDestinationValues(uint16_t dstIdx, std::span<float> output) const noexcept {
    ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
    if (dst_registry_[dstIdx].mode == ModDstMode::Mono) {
        if (!output.empty()) output.front() = mono_dst_[dstIdx];
        return 1;
    }

    // TODO: Remove the UI/audio data race on voice activity.
    const size_t voice_count = active_voices_.size();
    std::array<float, sample_width_v<Signal>> lanes{};
    size_t required = 0;
    size_t written = 0;

    for (size_t i = 0; i < voice_count; ++i) {
        const uint16_t voice = active_voices_[i];
        Mask mask = active_masks_[voice] & valid_mask_;
        required += std::popcount(mask);
        if (written == output.size()) continue;

        store_unaligned(poly_dst_buf_[static_cast<size_t>(voice) * poly_dst_stride_ + dstIdx], lanes.data());
        while (mask != 0 && written < output.size()) {
            const auto lane = std::countr_zero(mask);
            output[written++] = lanes[lane];
            mask &= mask - 1;
        }
    }

    return required;
}

inline uint16_t ModMatrixControl::allocateDepthSlot(float initial_depth) {
    for (size_t i = 0; i < program_.depth_active_.size(); ++i) {
        if (program_.depth_active_[i] != 0) continue;
        program_.depth_base_[i] = initial_depth;
        program_.depth_active_[i] = 1;
        return static_cast<uint16_t>(i);
    }

    ASSERT(program_.depth_base_.size() < config_.max_connections, "max_connections exceeded");
    program_.depth_base_.push_back(initial_depth);
    program_.depth_active_.push_back(1);
    return static_cast<uint16_t>(program_.depth_base_.size() - 1);
}

inline bool ModMatrixControl::dstIsConnected(uint16_t dstIdx) const {
    return std::ranges::any_of(connections_, [dstIdx](const auto& connection) {
        return !connection.isDepthMod() && connection.dst_idx == dstIdx;
    });
}

inline bool ModMatrixControl::srcIsConnected(uint16_t srcIdx) const {
    return std::ranges::any_of(connections_, [srcIdx](const auto& connection) { return connection.src_idx == srcIdx; });
}

inline void ModMatrixControl::recompileProgram() {
    program_.mm_connections.clear();
    program_.mp_connections.clear();
    program_.pm_connections.clear();
    program_.pp_connections.clear();
    program_.depth_connections_mono_.clear();
    program_.depth_connections_poly_.clear();

    for (const auto& connection : connections_) {
        const auto& source = src_registry_[connection.src_idx];
        const ModSrcMode source_mode = source.type == ModSrcType::Mono ? ModSrcMode::Mono
            : source.type == ModSrcType::Poly                          ? ModSrcMode::Poly
                                                                       : source.mode;

        uint8_t flags = 0;
        if (connection.isDepthMod()) flags |= ModConnectionHandle::kFlagDepthMod;
        if (source.bipolar) flags |= ModConnectionHandle::kFlagSrcBipolar;
        if (connection.isBipolar()) flags |= ModConnectionHandle::kFlagBipolar;
        const ModConnectionHandle handle{connection.src_idx, connection.dst_idx, connection.depth_slot, flags};

        if (connection.isDepthMod()) {
            auto& bucket =
                source_mode == ModSrcMode::Mono ? program_.depth_connections_mono_ : program_.depth_connections_poly_;
            bucket.push_back(handle);
            continue;
        }

        const ModDstMode destination_mode = dst_registry_[connection.dst_idx].mode;
        if (source_mode == ModSrcMode::Mono && destination_mode == ModDstMode::Mono)
            program_.mm_connections.push_back(handle);
        else if (source_mode == ModSrcMode::Mono && destination_mode == ModDstMode::Poly)
            program_.mp_connections.push_back(handle);
        else if (source_mode == ModSrcMode::Poly && destination_mode == ModDstMode::Poly)
            program_.pp_connections.push_back(handle);
        else
            program_.pm_connections.push_back(handle);
    }

    on_connections_changed();
}

inline float ModConnection::getDepth() const {
    ASSERT(matrix_, "Connection is not bound");
    return matrix_->getDepthBase(depth_slot);
}

inline void ModConnection::setDepth(float depth) {
    ASSERT(matrix_, "Connection is not bound");
    matrix_->setDepthBase(depth_slot, depth);
}

inline void ModConnection::setBipolar(bool bipolar) {
    ASSERT(matrix_, "Connection is not bound");
    if (isBipolar() == bipolar) return;
    flags = bipolar ? flags | kFlagBipolar : flags & ~kFlagBipolar;
    for (auto& connection : matrix_->connections_) {
        if (connection.depth_slot != depth_slot) continue;
        connection.flags = flags;
        break;
    }
    matrix_->recompileProgram();
}

inline const ModSource& ModConnection::source() const {
    ASSERT(matrix_, "Connection is not bound");
    return matrix_->getSource(src_idx);
}

inline const ModDestination* ModConnection::destination() const {
    ASSERT(matrix_, "Connection is not bound");
    return isDepthMod() ? nullptr : &matrix_->getDestination(dst_idx);
}

}  // namespace applause
