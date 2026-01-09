#pragma once

#include <string>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <optional>
#include <applause/extensions/ParamsExtension.h>
#include <applause/util/DebugHelpers.h>
#include <applause/util/ValueScaling.h>


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
    uint16_t index;
    ModSrcType type;
    ModSrcMode mode;
    bool bipolar;  ///< True if source naturally outputs [-1,+1], false for [0,1]
};

struct ModDestination {
    uint16_t index;
    ModDstMode mode;
};

/**
 * Represents a modulation connection between a source and destination.
 * The bipolar_mapping flag controls output behavior:
 * - bipolar_mapping=true:  Output centered at 0, can go negative [-depth, +depth]
 * - bipolar_mapping=false: Output only positive [0, +depth]
 */
struct ModConnection {
    uint16_t src;
    uint16_t dst;
    uint16_t depth_slot;
    bool bipolar_mapping;  ///< True for bidirectional modulation (wobble), false for additive only
};

/**
 * Represents a modulation connection that modulates the depth of another connection.
 */
struct DepthModConnection {
    uint16_t src;
    uint16_t depth_slot;
    float depth;
    bool bipolar_mapping;  ///< True for bidirectional modulation, false for additive only
};

/**
 * A lightweight connection handle that contains a source index, destination index, and depth slot index.
 * Used for internal modulation graph processing, instead of the heavier ModConnection class.
 * Caches src_bipolar from the source registry for efficient processing.
 */
struct ModConnectionHandle {
    uint16_t src;
    uint16_t dst;
    uint16_t depth_slot;
    bool src_bipolar;      ///< Cached: true if source outputs [-1,+1]
    bool bipolar_mapping;  ///< True for bidirectional modulation output
};

/**
 * Lightweight handle for depth modulation connections.
 */
struct DepthConnectionHandle {
    uint16_t src;
    uint16_t depth_slot;
    float depth;
    bool src_bipolar;      ///< Cached: true if source outputs [-1,+1]
    bool bipolar_mapping;  ///< True for bidirectional modulation output
};

/**
 * Lightweight handle for direct access to a modulated parameter value.
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
 *
 * Deeper modulation graphs would necessitate development of a signal digraph processing system, a graph compiler,
 * loop detection & resolution, et cetera. We omit this for now, as deeper recursive modulation (i.e. modulation of a connection
 * that modulates a connection that modulates a parameter) is an edge case not required within most synthesis applications.
 *
 * This might be nice in the future, though!
 *
 */
template<uint16_t NumVoices, uint16_t MaxSources, uint16_t MaxDestinations, uint16_t MaxConnections>
class ModMatrix;

/**
 * Represents a "compiled" modulation graph that can be executed efficiently. A ModMatrix owns exactly one of these.
 * When the user changes the modulation graph, this object is modified/replaced.
 *
 * We keep it as a separate object to atomically swap the whole thing all at the same time when an update is made,
 * so the DSP thread never reads from a partially-updated modulation system. Thread safety!
 */
template<uint16_t NumVoices, uint16_t MaxSources, uint16_t MaxDestinations, uint16_t MaxConnections>
class ModProgram {
    ModMatrix<NumVoices, MaxSources, MaxDestinations, MaxConnections> &matrix;

    std::vector<ModConnectionHandle> mm_connections;
    std::vector<ModConnectionHandle> mp_connections;
    std::vector<ModConnectionHandle> pm_connections;
    std::vector<ModConnectionHandle> pp_connections;

    uint16_t num_depth_connections_;
    std::vector<float> depth_base_;
    std::vector<uint8_t> depth_active_;
    std::vector<DepthConnectionHandle> depth_connections_mono_;
    std::vector<DepthConnectionHandle> depth_connections_poly_;

public:
    explicit ModProgram(ModMatrix<NumVoices, MaxSources, MaxDestinations, MaxConnections> &matrix)
        : matrix(matrix) {
    }

    friend class ModMatrix<NumVoices, MaxSources, MaxDestinations, MaxConnections>;
};


template<uint16_t NumVoices, uint16_t MaxSources, uint16_t MaxDestinations, uint16_t MaxConnections>
class ModMatrix {
public:
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
    ModSource& registerSource(const std::string& string_id, ModSrcType type,
                              bool bipolar = false,
                              ModSrcMode defaultMode = ModSrcMode::Poly)
    {
        ASSERT(src_count_ < MaxSources, "MaxSources exceeded");
        ASSERT(!src_lookup_.contains(string_id), "Source name already registered");

        const auto idx = static_cast<uint16_t>(src_count_++);
        src_lookup_.emplace(string_id, idx);

        ModSource& s = src_registry_[idx];
        s.index = idx;
        s.type  = type;
        s.mode  = (type == ModSrcType::Both) ? defaultMode
               : (type == ModSrcType::Poly) ? ModSrcMode::Poly
                                            : ModSrcMode::Mono;
        s.bipolar = bipolar;
        return s;
    }

    /**
     * Sets the mode for a source that supports both mono and poly operation.
     * This triggers a recompilation of the modulation program to update connection routing.
     * @param srcIdx The source index
     * @param mode The new mode (Mono or Poly)
     * @note Only valid for sources registered with ModSrcType::Both
     */
    void setSourceMode(uint16_t srcIdx, ModSrcMode mode) {
        ASSERT(srcIdx < src_count_, "Source index out of bounds");
        ASSERT(src_registry_[srcIdx].type == ModSrcType::Both,
               "setSourceMode only valid for sources with ModSrcType::Both");
        src_registry_[srcIdx].mode = mode;
        recompileProgram();
    }

    /**
     * Registers a new modulation destination.
     * @param string_id unique identifier for the destination
     * @param mode whether the destination is mono or poly
     * @param scale_info scaling info for converting normalized <-> plain values.
     *        Defaults to identity scaling (min=0, max=1, Linear).
     * @return reference to the registered destination
     */
    ModDestination& registerDestination(const std::string& string_id, ModDstMode mode,
                                        applause::ValueScaleInfo scale_info = {0.0f, 1.0f, applause::ValueScaling::linear()})
    {
        ASSERT(dst_count_ < MaxDestinations, "MaxDestinations exceeded");
        ASSERT(!dst_lookup_.contains(string_id), "Destination name already registered");

        const auto idx = static_cast<uint16_t>(dst_count_++);
        dst_lookup_.emplace(string_id, idx);

        ModDestination& d = dst_registry_[idx];
        d.index = idx;
        d.mode  = mode;
        dst_scale_info_[idx] = scale_info;

        if (mode == ModDstMode::Poly) {
            poly_dst_indices_.push_back(idx);
        }

        return d;
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
    void registerFromParamsExtension(const applause::ParamsExtension& params_extension) {
        ASSERT(dst_count_ == 0, "Cannot batch register parameters from extension after manually registering destinations");
        const auto* scale_array = params_extension.getScaleInfoArray();
        const auto& params = params_extension.getAllParameters();

        for (uint32_t i = 0; i < params.size(); ++i) {
            const auto& param = params[i];
            ModDstMode mode = param.polyphonic ? ModDstMode::Poly : ModDstMode::Mono;
            registerDestination(param.stringId, mode, scale_array[i]);
        }
    }

    /**
     * Creates a new modulation connection between a source and a destination. If an existing connection exists, it will
     * be replaced.
     *
     * @param src the modulation source
     * @param dst the modulation destination
     * @param depth the initial modulation depth
     * @param bipolar_mapping whether the output should be centered at 0 (bidirectional).
     *        If not specified, defaults to the source's bipolar flag (LFOs default to bidirectional,
     *        envelopes default to additive-only).
     * @return reference to the connection
     */
    ModConnection& addConnection(ModSource src, ModDestination dst, float depth,
                                 std::optional<bool> bipolar_mapping = std::nullopt) {
        ASSERT(src.index < src_count_, "Source index out of bounds");
        ASSERT(dst.index < dst_count_, "Destination index out of bounds");
        // Default to source's bipolar flag if not specified
        const bool mapping = bipolar_mapping.value_or(src_registry_[src.index].bipolar);

        // Check for existing connection with same (src, dst)
        for (auto& existing : connections_) {
            if (existing.src == src.index && existing.dst == dst.index) {
                // Update existing connection
                program_.depth_base_[existing.depth_slot] = depth;
                existing.bipolar_mapping = mapping;
                recompileProgram();
                return existing;
            }
        }

        // No existing connection found - create new one
        ASSERT(program_.depth_base_.size() < MaxConnections, "MaxConnections/depth slots exceeded");
        ModConnection connection{};
        connection.src = src.index;
        connection.dst = dst.index;
        connection.bipolar_mapping = mapping;
        program_.depth_base_.push_back(depth);
        program_.depth_active_.push_back(1);
        connection.depth_slot = static_cast<uint16_t>(program_.depth_base_.size() - 1);
        connections_.push_back(connection);

        recompileProgram();
        return connections_.back();
    }

    /**
     * Adds a modulation connection that modulates the depth of an existing connection.
     *
     * @param src the modulation source
     * @param depth_slot the depth slot to modulate (from an existing connection)
     * @param depth the modulation depth for this depth-modulation
     * @param bipolar_mapping whether the output should be centered at 0 (bidirectional).
     *        If not specified, defaults to the source's bipolar flag.
     * @return reference to the depth modulation connection
     */
    DepthModConnection& addDepthModulation(ModSource src, uint16_t depth_slot, float depth,
                                            std::optional<bool> bipolar_mapping = std::nullopt) {
        ASSERT(src.index < src_count_, "Source index out of bounds");
        ASSERT(depth_slot < program_.depth_base_.size(), "Invalid depth slot");
        const bool mapping = bipolar_mapping.value_or(src_registry_[src.index].bipolar);

        DepthModConnection connection{};
        connection.src = src.index;
        connection.depth_slot = depth_slot;
        connection.depth = depth;
        connection.bipolar_mapping = mapping;
        depth_connections_.push_back(connection);

        recompileProgram();
        return depth_connections_.back();
    }

    /**
     * Notifies the matrix that the voice corresponding to voice_index has just been activated.
     * This function must be called whenever a voice is triggered by, e.g., a fresh incoming MIDI note
     *
     * The matrix uses this information to selectively
     * process polyphonic modulation paths, as well as handle poly->mono source->destination routing policy.
     */
    void notifyVoiceOn(uint16_t voice_index) {
        ASSERT(voice_index < NumVoices, "voice_index out of bounds");
        if (std::find(active_voices_.begin(), active_voices_.end(), voice_index) == active_voices_.end()) {
            active_voices_.push_back(voice_index);
        }
    }

    /**
     * Notifies the matrix that the voice corresponding to voice_index has been deactivated and is no longer
     * being processed nor producing audio. This function must be called whenever a voice is disabled.
     */
    void notifyVoiceOff(uint16_t voice_index) {
        std::erase(active_voices_, voice_index);
    }

    /**
     * Sets the base (unmodulated) value for a destination in plain units.
     * The value is internally normalized using the destination's scaling info.
     */
    void setBaseValue(uint16_t dstIdx, float plain_value) {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        const auto& s = dst_scale_info_[dstIdx];
        float norm = s.scaling.toNormalized(plain_value, s.min, s.max);
        base_mono_dst_[dstIdx] = norm;
        base_poly_dst_[dstIdx] = norm;
    }

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
        ASSERT(voice < NumVoices, "Voice index out of bounds");
        poly_src_buf_[static_cast<size_t>(voice) * MaxSources + srcIdx] = value;
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
        ASSERT(voice < NumVoices, "Voice index out of bounds");
        mono_src_buf_[srcIdx] = value;
        poly_src_buf_[static_cast<size_t>(voice) * MaxSources + srcIdx] = value;
    }

    /**
     * Returns the final modulated value for a mono destination in plain units.
     * Call this after process() to retrieve the modulated parameter value.
     * The value is already scaled and ready for DSP use.
     */
    [[nodiscard]] float getModValue(uint16_t dstIdx) const {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        return mono_dst_[dstIdx];
    }

    /**
     * Returns the final modulated value for a poly destination for a specific voice, in plain units.
     * Call this after process() to retrieve the modulated parameter value.
     * The value is already scaled and ready for DSP use.
     */
    [[nodiscard]] float getPolyModValue(uint16_t dstIdx, uint16_t voice) const {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        ASSERT(voice < NumVoices, "Voice index out of bounds");
        return poly_dst_buf_[static_cast<size_t>(voice) * MaxDestinations + dstIdx];
    }

    /**
     * Get a handle for direct access to a mono destination's modulated value.
     * @param dstIdx The destination index
     * @return Handle pointing directly to the value in mono_dst_
     * @note Cache this handle during initialization; don't call every process()
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
     * @note Cache this handle during initialization; don't call every process()
     */
    [[nodiscard]] ModParamHandle getModHandle(uint16_t dstIdx, uint16_t voice) {
        ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
        ASSERT(voice < NumVoices, "Voice index out of bounds");
        return ModParamHandle{&poly_dst_buf_[static_cast<size_t>(voice) * MaxDestinations + dstIdx]};
    }

    /**
     * Load all param values as normalized into base destination values.
     * Assumes param index == destination index (1:1 bijection).
     * Call once per block before process().
     */
    void loadParamBaseValues(const applause::ParamsExtension& params) {
        const auto* values = params.getValuesArray();
        const auto* scales = params.getScaleInfoArray();

        for (uint16_t i = 0; i < dst_count_; i++) {
            float plain = values[i].load(std::memory_order_relaxed);
            const auto& s = scales[i];
            float norm = s.scaling.toNormalized(plain, s.min, s.max);
            base_mono_dst_[i] = norm;
            base_poly_dst_[i] = norm;
        }
    }

    /**
     * Processes modulation. Should be called once per block, before parameters are read and used by DSP components.
     */
    void process() {
        // Reset mono destinations to base values
        mono_dst_ = base_mono_dst_;

        // Reset poly destinations for active voices (only poly destinations need per-voice reset)
        for (size_t i = 0; i < active_voices_.size(); i++) {
            const uint16_t voice_index = active_voices_[i];
            const size_t voice_offset = static_cast<size_t>(voice_index) * MaxDestinations;
            for (uint16_t poly_idx : poly_dst_indices_) {
                poly_dst_buf_[voice_offset + poly_idx] = base_poly_dst_[poly_idx];
            }
        }

        // load base depth values into the mono depth buffer
        for (size_t slot = 0; slot < program_.depth_base_.size(); ++slot) {
            mono_depth_buf_[slot] = program_.depth_active_[slot] ? program_.depth_base_[slot] : 0.0f;
        }

        // calculate mono depth modulation
        for (const auto &mono_depth_conn: program_.depth_connections_mono_) {
            float src_val = mono_src_buf_[mono_depth_conn.src];
            // Step 1: Normalize bipolar sources to [0,1]
            if (mono_depth_conn.src_bipolar) {
                src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
            }
            // Step 2: Apply output mapping
            if (mono_depth_conn.bipolar_mapping) {
                src_val = src_val * 2.0f - 1.0f;   // [0,1] -> [-1,+1]
            }
            mono_depth_buf_[mono_depth_conn.depth_slot] += src_val * mono_depth_conn.depth;
        }

        // load poly depth from mono_depth_buf_
        for (uint16_t i = 0; i < active_voices_.size(); i++) {
            const uint16_t voice_index = active_voices_[i];
            for (uint16_t j = 0; j < program_.depth_base_.size(); j++) {
                poly_depth_buf_[static_cast<size_t>(voice_index) * MaxConnections + j] = mono_depth_buf_[j];
            }
        }

        // load poly depth modulation into the poly depth buffer
        for (uint16_t i = 0; i < active_voices_.size(); i++) {
            uint16_t voice_index = active_voices_[i];
            for (const auto &poly_depth_conn: program_.depth_connections_poly_) {
                float src_val = poly_src_buf_[static_cast<size_t>(voice_index) * MaxSources + poly_depth_conn.src];
                // Step 1: Normalize bipolar sources to [0,1]
                if (poly_depth_conn.src_bipolar) {
                    src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
                }
                // Step 2: Apply output mapping
                if (poly_depth_conn.bipolar_mapping) {
                    src_val = src_val * 2.0f - 1.0f;   // [0,1] -> [-1,+1]
                }
                poly_depth_buf_[static_cast<size_t>(voice_index) * MaxConnections + poly_depth_conn.depth_slot]
                        += src_val * poly_depth_conn.depth;
            }
        }

        // now that the modulation depth pass is done, we can calculate final modulation values
        // first, mono -> mono connections
        // NOTE: MM connections read from mono_depth_buf_, so poly depth modulation on these
        // depth slots is silently ignored. This is analogous to PM connections (NYI) - both
        // require a reduction policy to collapse per-voice values to mono. Until implemented,
        // avoid poly depth mods on slots used by MM connections.
        for (const auto &mm_conn: program_.mm_connections) {
            float src_val = mono_src_buf_[mm_conn.src];
            // Step 1: Normalize bipolar sources to [0,1]
            if (mm_conn.src_bipolar) {
                src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
            }
            // Step 2: Apply output mapping
            if (mm_conn.bipolar_mapping) {
                src_val = src_val * 2.0f - 1.0f;   // [0,1] -> [-1,+1]
            }
            const float depth_val = mono_depth_buf_[mm_conn.depth_slot];
            mono_dst_[mm_conn.dst] += src_val * depth_val;
        }

        // mono -> poly connections
        for (uint16_t i = 0; i < active_voices_.size(); i++) {
            uint16_t voice_index = active_voices_[i];
            for (const auto &mp_conn: program_.mp_connections) {
                float src_val = mono_src_buf_[mp_conn.src];
                // Step 1: Normalize bipolar sources to [0,1]
                if (mp_conn.src_bipolar) {
                    src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
                }
                // Step 2: Apply output mapping
                if (mp_conn.bipolar_mapping) {
                    src_val = src_val * 2.0f - 1.0f;   // [0,1] -> [-1,+1]
                }
                const float depth_val = poly_depth_buf_[
                    static_cast<size_t>(voice_index) * MaxConnections + mp_conn.depth_slot];
                poly_dst_buf_[static_cast<size_t>(voice_index) * MaxDestinations + mp_conn.dst] += src_val * depth_val;
            }
        }

        // poly -> poly connections
        for (uint16_t i = 0; i < active_voices_.size(); i++) {
            uint16_t voice_index = active_voices_[i];
            for (const auto &pp_conn: program_.pp_connections) {
                float src_val = poly_src_buf_[static_cast<size_t>(voice_index) * MaxSources + pp_conn.src];
                // Step 1: Normalize bipolar sources to [0,1]
                if (pp_conn.src_bipolar) {
                    src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
                }
                // Step 2: Apply output mapping
                if (pp_conn.bipolar_mapping) {
                    src_val = src_val * 2.0f - 1.0f;   // [0,1] -> [-1,+1]
                }
                const float depth_val = poly_depth_buf_[
                    static_cast<size_t>(voice_index) * MaxConnections + pp_conn.depth_slot];
                poly_dst_buf_[static_cast<size_t>(voice_index) * MaxDestinations + pp_conn.dst] += src_val * depth_val;
            }
        }

        // poly -> mono connections (NYI -- leave as stub)


        // Scale mono destinations: normalized -> plain
        for (uint16_t i = 0; i < dst_count_; i++) {
            const auto& s = dst_scale_info_[i];
            float norm = std::clamp(mono_dst_[i], 0.0f, 1.0f);
            mono_dst_[i] = s.scaling.fromNormalized(norm, s.min, s.max);
        }

        // Scale poly destinations for active voices: normalized -> plain
        // Only iterate over poly destinations - mono destinations don't need per-voice scaling
        for (size_t v = 0; v < active_voices_.size(); v++) {
            const uint16_t voice_index = active_voices_[v];
            const size_t voice_offset = static_cast<size_t>(voice_index) * MaxDestinations;

            for (uint16_t poly_idx : poly_dst_indices_) {
                const auto& s = dst_scale_info_[poly_idx];
                float norm = std::clamp(poly_dst_buf_[voice_offset + poly_idx], 0.0f, 1.0f);
                poly_dst_buf_[voice_offset + poly_idx] = s.scaling.fromNormalized(norm, s.min, s.max);
            }
        }
    }

private:
    /**
     * Returns the effective source mode for a given source index.
     * - ModSrcType::Mono sources always return Mono (regardless of mode field)
     * - ModSrcType::Poly sources always return Poly (regardless of mode field)
     * - ModSrcType::Both sources return their current toggle state (mode field)
     */
    [[nodiscard]] ModSrcMode effectiveSrcMode(uint16_t srcIdx) const {
        const auto& s = src_registry_[srcIdx];
        if (s.type == ModSrcType::Mono) return ModSrcMode::Mono;
        if (s.type == ModSrcType::Poly) return ModSrcMode::Poly;
        return s.mode;  // ModSrcType::Both - use toggle state
    }

    /**
     * Rebuilds the modulation program object. Called whenever the user changes the modulation matrix state.
     */
    void recompileProgram() {
        program_.mm_connections.clear();
        program_.mp_connections.clear();
        program_.pm_connections.clear();
        program_.pp_connections.clear();

        program_.depth_connections_mono_.clear();
        program_.depth_connections_poly_.clear();

        for (const auto &dm : depth_connections_) {
            const bool src_bipolar = src_registry_[dm.src].bipolar;
            const DepthConnectionHandle handle = {dm.src, dm.depth_slot, dm.depth, src_bipolar, dm.bipolar_mapping};
            if (effectiveSrcMode(dm.src) == ModSrcMode::Mono) {
                program_.depth_connections_mono_.push_back(handle);
            } else {
                program_.depth_connections_poly_.push_back(handle);
            }
        }

        for (const auto &conn: connections_) {
            const ModSrcMode src_mode = effectiveSrcMode(conn.src);
            const ModDstMode dst_mode = dst_registry_[conn.dst].mode;
            const bool src_bipolar = src_registry_[conn.src].bipolar;

            ModConnectionHandle handle = {conn.src, conn.dst, conn.depth_slot, src_bipolar, conn.bipolar_mapping};

            if (src_mode == ModSrcMode::Mono && dst_mode == ModDstMode::Mono) {
                program_.mm_connections.push_back(handle);
            } else if (src_mode == ModSrcMode::Mono && dst_mode == ModDstMode::Poly) {
                program_.mp_connections.push_back(handle);
            } else if (src_mode == ModSrcMode::Poly && dst_mode == ModDstMode::Poly) {
                program_.pp_connections.push_back(handle);
            } else if (src_mode == ModSrcMode::Poly && dst_mode == ModDstMode::Mono) {
                program_.pm_connections.push_back(handle);
            }
        }
    }

    ModProgram<NumVoices, MaxSources, MaxDestinations, MaxConnections> program_{*this};

    int src_count_ = 0;
    int dst_count_ = 0;

    std::vector<uint16_t> active_voices_;

    std::unordered_map<std::string, uint16_t> src_lookup_;
    std::unordered_map<std::string, uint16_t> dst_lookup_;

    std::array<ModSource, MaxSources> src_registry_{};
    std::array<ModDestination, MaxDestinations> dst_registry_{};
    std::array<applause::ValueScaleInfo, MaxDestinations> dst_scale_info_{};
    std::vector<uint16_t> poly_dst_indices_;  // Indices of poly destinations for optimized iteration

    // Source values (written by your modulators before processBlock)
    std::array<float, MaxSources> mono_src_buf_{};
    std::array<float, size_t(NumVoices) * MaxSources> poly_src_buf_{};

    // Base destination values (unmodulated knob values, optional)
    std::array<float, MaxDestinations> base_mono_dst_{};
    std::array<float, MaxDestinations> base_poly_dst_{};

    std::array<float, MaxConnections> mono_depth_buf_{};
    std::array<float, size_t(NumVoices) * MaxConnections> poly_depth_buf_{};

    std::array<float, MaxDestinations> mono_dst_{};
    std::array<float, size_t(NumVoices) * MaxDestinations> poly_dst_buf_{};

    std::vector<ModConnection> connections_;
    std::vector<DepthModConnection> depth_connections_;

    friend class ModProgram<NumVoices, MaxSources, MaxDestinations, MaxConnections>;
};
