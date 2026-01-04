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

    DepthModConnection addDepthModulation(ModSrcId src, ModConnection dst, float depth, bool bipolar) {
        ASSERT(program_.depthBase.size() < MaxConnections, "MaxConnections/depth slots exceeded");

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
