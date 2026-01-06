#pragma once
#include <cstdint>
#include <array>
#include <map>
#include <vector>
#include <applause/util/DebugHelpers.h>


enum class ModSrcType : uint8_t { Mono, Poly, Both };

enum class ModSrcMode : uint8_t {Mono, Poly};

enum class DepthModConnectionType : uint8_t { Mono, Poly };

// special handling for mod connection types not yet implemented,
// for now we just do mono sources and poly sources
enum class ModConnectionType : uint8_t {
    MonoToMono, MonoToPoly, PolyToPoly, PolyToMono
};

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
struct ModSrcId {
    uint16_t index;
    ModSrcType type;
    ModSrcMode mode;
};

struct ModDstId {
    uint16_t index;
};

struct ModConnection {
    ModConnectionType type;
    ModSrcId src;
    ModDstId dst;
    uint16_t depth_slot;
    bool bipolar;
};

struct DepthModConnection {
    ModDstId dst;
    float depth;
    DepthModConnectionType type;
};

struct ConnectionHandle {
    uint16_t src;
    uint16_t dst;
    float depth;
};

template<uint16_t NumVoices, uint16_t MaxSources, uint16_t MaxDestinations, uint16_t MaxConnections>
class ModMatrix;

/**
 * Represents a "compiled" modulation graph that can be executed efficiently. A ModMatrix owns exactly one of these.
 * When the user changes the modulation graph, this object is modified/replaced.
 */
template<uint16_t NumVoices, uint16_t MaxSources, uint16_t MaxDestinations, uint16_t MaxConnections>
class ModProgram {

    ModMatrix<NumVoices, MaxSources, MaxDestinations, MaxConnections>& matrix;

    std::vector<ConnectionHandle> mm_connections;
    std::vector<ConnectionHandle> mp_connections;
    std::vector<ConnectionHandle> pm_connections;
    std::vector<ConnectionHandle> pp_connections;

    std::vector<ConnectionHandle> depth_connections_mono_;
    std::vector<ConnectionHandle> depth_connections_poly_;

public:
    explicit ModProgram(ModMatrix<NumVoices, MaxSources, MaxDestinations, MaxConnections> &matrix)
        : matrix(matrix) {
    }

private:
    void process() {

    }
};


template<uint16_t NumVoices, uint16_t MaxSources, uint16_t MaxDestinations, uint16_t MaxConnections>
class ModMatrix {
public:
    /**
     * Registers a new modulation source symbol uniquely identifiable via string_id. This function only registers the
     * source symbolically within the matrix; to link modulation channels to your signal generators (LFOs, envelopes,
     * etc) you should invoke registerMonoSourceChannel/
     * @param string_id the unique identifier of the source
     * @param type
     * @return
     */
    ModSrcId& registerSource(std::string string_id, ModSrcType type) {
        ASSERT(src_count_ < MaxSources, "MaxSources exceeded");
        ASSERT(!src_registry_.contains(string_id), "Source name already registered");

        ModSrcId& id = src_registry_[string_id];
        id.index = src_count_ - 1;

        src_count_ += 1;
        return id;
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
    ModConnection addConnection(ModSrcId src, ModDstId dst, float depth, bool bipolar) {
        ModConnection connection{};
        connection.src = src;
        connection.dst = dst;
        connection.bipolar = bipolar;
    }
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
     * Rebuilds the modulation program object
     */
    void recompileProgram() {
        program_.mm_connections.clear();
        program_.mp_connections.clear();
        program_.pm_connections.clear();
        program_.pp_connections.clear();

        program_.depth_connections_mono_.clear();
        program_.depth_connections_poly_.clear();
    }

    ModProgram<NumVoices, MaxSources, MaxDestinations, MaxConnections> program_{*this};

    int src_count_ = 0;
    int dst_count_ = 0;

    std::unordered_map<std::string, ModSrcId> src_registry_;
    std::unordered_map<std::string, ModDstId> dst_registry_;

    // Source values (written by your modulators before processBlock)
    std::array<float, MaxSources> mono_src_buf_{};
    std::array<float, size_t(NumVoices) * MaxSources> poly_src_buf_{};

    // Base destination values (unmodulated knob values, optional)
    std::array<float, MaxDestinations> base_mono_dst_{};
    std::array<float, MaxDestinations> base_poly_dst_{};                         // [dst] (broadcast per voice)

    std::array<float, MaxConnections> mono_depth_buf_{};                             // [slot] base + mono depth mods
    std::array<float, size_t(NumVoices) * MaxConnections> poly_depth_buf{};              // [voice][slot] includes poly depth mods

    std::array<float, MaxDestinations> mono_dst_{};                              // [dst]
    std::array<float, size_t(NumVoices) * MaxDestinations> poly_dst_buf_{};          // [voice][dst]

    std::vector<ModConnection> connections_;
    std::vector<DepthModConnection> depth_connections_;

    friend class ModProgram<NumVoices, MaxSources, MaxDestinations, MaxConnections>;
};
