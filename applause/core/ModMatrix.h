#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <array>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <applause/util/DebugHelpers.h>


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
 */
struct ModSource {
    uint16_t index;
    ModSrcType type;
    ModSrcMode mode;
};

struct ModDestination {
    uint16_t index;
    ModDstMode mode;
    float min_value;
    float max_value;
};

struct ModConnection {
    uint16_t src;
    uint16_t dst;
    uint16_t depth_slot;
    bool bipolar;
};

struct DepthModConnection {
    uint16_t src;
    uint16_t depth_slot;
    float depth;
    bool bipolar;
};

/**
 * A lightweight connection handle that contains a source index, destination index, and depth slot index.
 * Used for internal modulation graph processing, instead of the heavier ModConnection class.
 */
struct ModConnectionHandle {
    uint16_t src;
    uint16_t dst;
    uint16_t depth_slot;
    bool bipolar;
};

struct DepthConnectionHandle {
    uint16_t src;
    uint16_t depth_slot;
    float depth;
    bool bipolar;
};

/**
 * A fast and efficient parameter modulation system. Supports modulation from mod sources (e.g. LFOs) to destinations
 * (e.g. synth parameters). The depth of individual connections themselves can also be modulated;
 * in other words, the matrix supports depth-one modulation-of-modulation.
 *
 * The modulation system is designed independently of the Applause parameter module. While there is a natural
 * bijection between plugin parameters and modulation destinations (and we supply some glue code to facilitate this),
 * no such association is strictly necessary.
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
     * @param type
     * @return
     */
    ModSource& registerSource(const std::string& string_id, ModSrcType type,
                              ModSrcMode defaultMode = ModSrcMode::Poly)
    {
        ASSERT(src_count_ < MaxSources, "MaxSources exceeded");
        ASSERT(!src_lookup_.contains(string_id), "Source name already registered");

        const uint16_t idx = static_cast<uint16_t>(src_count_++);
        src_lookup_.emplace(string_id, idx);

        ModSource& s = src_registry_[idx];
        s.index = idx;
        s.type  = type;
        s.mode  = (type == ModSrcType::Both) ? defaultMode
               : (type == ModSrcType::Poly) ? ModSrcMode::Poly
                                            : ModSrcMode::Mono;
        return s;
    }

    ModDestination& registerDestination(const std::string& string_id, ModDstMode mode,
                                        float min_value = 0.0f, float max_value = 1.0f)
    {
        ASSERT(dst_count_ < MaxDestinations, "MaxDestinations exceeded");
        ASSERT(!dst_lookup_.contains(string_id), "Destination name already registered");

        const auto idx = static_cast<uint16_t>(dst_count_++);
        dst_lookup_.emplace(string_id, idx);

        ModDestination& d = dst_registry_[idx];
        d.index = idx;
        d.mode  = mode;
        d.min_value = min_value;
        d.max_value = max_value;
        return d;
    }

    /**
     * Creates a new modulation connection between a source and a destination. If an existing connection exists, it will
     * be replaced.
     *
     * @param src the modulation source
     * @param dst the modulation destination
     * @param depth the initial modulation depth
     * @param bipolar whether the connection is bipolar or not; modulation will be clamped between [-depth, depth] or [0, depth] depending on polarity
     * @return
     */
    ModConnection& addConnection(ModSource src, ModDestination dst, float depth, bool bipolar) {
        // Check for existing connection with same (src, dst)
        for (auto& existing : connections_) {
            if (existing.src == src.index && existing.dst == dst.index) {
                // Update existing connection
                program_.depth_base_[existing.depth_slot] = depth;
                existing.bipolar = bipolar;
                recompileProgram();
                return existing;
            }
        }

        // No existing connection found - create new one
        ASSERT(program_.depth_base_.size() < MaxConnections, "MaxConnections/depth slots exceeded");
        ModConnection connection{};
        connection.src = src.index;
        connection.dst = dst.index;
        connection.bipolar = bipolar;
        program_.depth_base_.push_back(depth);
        program_.depth_active_.push_back(1);
        connection.depth_slot = static_cast<uint16_t>(program_.depth_base_.size() - 1);
        connections_.push_back(connection);

        recompileProgram();
        return connections_.back();
    }

    DepthModConnection& addDepthModulation(ModSource src, uint16_t depth_slot, float depth, bool bipolar) {
        ASSERT(depth_slot < program_.depth_base_.size(), "Invalid depth slot");
        DepthModConnection connection{};
        connection.src = src.index;
        connection.depth_slot = depth_slot;
        connection.depth = depth;
        connection.bipolar = bipolar;
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
     * Sets the base (unmodulated) value for a destination. Pass the plain value (e.g. Hz, dB);
     * it will be normalized internally for modulation processing.
     */
    void setBaseValue(uint16_t dstIdx, float plain_value) {
        float normalized = normalizeValue(dstIdx, plain_value);
        base_mono_dst_[dstIdx] = normalized;
        base_poly_dst_[dstIdx] = normalized;
    }

    /**
     * Returns the final modulated value for a mono destination as a plain value (denormalized).
     * Call this after process() to retrieve the modulated parameter value.
     */
    [[nodiscard]] float getModValue(uint16_t dstIdx) const {
        return denormalizeValue(dstIdx, mono_dst_[dstIdx]);
    }

    /**
     * Returns the final modulated value for a poly destination for a specific voice, as a plain value (denormalized).
     * Call this after process() to retrieve the modulated parameter value.
     */
    [[nodiscard]] float getPolyModValue(uint16_t dstIdx, uint16_t voice) const {
        float normalized = poly_dst_buf_[static_cast<size_t>(voice) * MaxDestinations + dstIdx];
        return denormalizeValue(dstIdx, normalized);
    }

    /**
     * Processes modulation. Should be called once per block, before parameters are read and used by DSP components.
     */
    void process() {
        // Reset mono destinations to base values
        mono_dst_ = base_mono_dst_;

        // Reset poly destinations for active voices
        for (size_t i = 0; i < active_voices_.size(); i++) {
            const uint16_t voice_index = active_voices_[i];
            for (int j = 0; j < dst_count_; j++) {
                poly_dst_buf_[static_cast<size_t>(voice_index) * MaxDestinations + j] = base_poly_dst_[j];
            }
        }

        // load base depth values into the mono depth buffer
        for (size_t slot = 0; slot < program_.depth_base_.size(); ++slot) {
            mono_depth_buf_[slot] = program_.depth_active_[slot] ? program_.depth_base_[slot] : 0.0f;
        }

        // calculate mono depth modulation
        for (const auto &mono_depth_conn: program_.depth_connections_mono_) {
            float src_val = mono_src_buf_[mono_depth_conn.src];
            if (mono_depth_conn.bipolar) {
                src_val = src_val * 2.0f - 1.0f;  // [0,1] -> [-1,1]
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
                if (poly_depth_conn.bipolar) {
                    src_val = src_val * 2.0f - 1.0f;  // [0,1] -> [-1,1]
                }
                poly_depth_buf_[static_cast<size_t>(voice_index) * MaxConnections + poly_depth_conn.depth_slot]
                        += src_val * poly_depth_conn.depth;
            }
        }

        // now that the modulation depth pass is done, we can calculate final modulation values
        // first, mono -> mono connections
        for (const auto &mm_conn: program_.mm_connections) {
            float src_val = mono_src_buf_[mm_conn.src];
            if (mm_conn.bipolar) {
                src_val = src_val * 2.0f - 1.0f;  // [0,1] -> [-1,1]
            }
            const float depth_val = mono_depth_buf_[mm_conn.depth_slot];
            mono_dst_[mm_conn.dst] += src_val * depth_val;
        }

        // mono -> poly connections
        for (uint16_t i = 0; i < active_voices_.size(); i++) {
            uint16_t voice_index = active_voices_[i];
            for (const auto &mp_conn: program_.mp_connections) {
                float src_val = mono_src_buf_[mp_conn.src];
                if (mp_conn.bipolar) {
                    src_val = src_val * 2.0f - 1.0f;  // [0,1] -> [-1,1]
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
                if (pp_conn.bipolar) {
                    src_val = src_val * 2.0f - 1.0f;  // [0,1] -> [-1,1]
                }
                const float depth_val = poly_depth_buf_[
                    static_cast<size_t>(voice_index) * MaxConnections + pp_conn.depth_slot];
                poly_dst_buf_[static_cast<size_t>(voice_index) * MaxDestinations + pp_conn.dst] += src_val * depth_val;
            }
        }

        // poly -> mono connections (NYI -- leave as stub)
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
     * Normalize a plain value to [0,1] range based on destination's min/max.
     */
    [[nodiscard]] float normalizeValue(uint16_t dstIdx, float plain_value) const {
        const auto& d = dst_registry_[dstIdx];
        return (plain_value - d.min_value) / (d.max_value - d.min_value);
    }

    /**
     * Denormalize a [0,1] value to plain value, clamped to destination's range.
     */
    [[nodiscard]] float denormalizeValue(uint16_t dstIdx, float normalized) const {
        const auto& d = dst_registry_[dstIdx];
        float plain = normalized * (d.max_value - d.min_value) + d.min_value;
        return std::clamp(plain, d.min_value, d.max_value);
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
            const DepthConnectionHandle handle = {dm.src, dm.depth_slot, dm.depth, dm.bipolar};
            if (effectiveSrcMode(dm.src) == ModSrcMode::Mono) {
                program_.depth_connections_mono_.push_back(handle);
            } else {
                program_.depth_connections_poly_.push_back(handle);
            }
        }

        for (const auto &conn: connections_) {
            const ModSrcMode src_mode = effectiveSrcMode(conn.src);
            const ModDstMode dst_mode = dst_registry_[conn.dst].mode;

            ModConnectionHandle handle = {conn.src, conn.dst, conn.depth_slot, conn.bipolar};

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
