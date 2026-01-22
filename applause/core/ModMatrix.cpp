#include "ModMatrix.h"

namespace applause {

ModSource& ModMatrix::registerSource(const std::string& string_id, ModSrcType type, bool bipolar,
                                     ModSrcMode defaultMode) {
    ASSERT(src_count_ < config_.max_sources, "max_sources exceeded");
    ASSERT(!src_lookup_.contains(string_id), "Source name already registered");

    const auto idx = static_cast<uint16_t>(src_count_++);
    src_lookup_.emplace(string_id, idx);

    ModSource& s = src_registry_[idx];
    s.name = string_id;
    s.index = idx;
    s.type = type;
    s.mode = (type == ModSrcType::Both) ? defaultMode
        : (type == ModSrcType::Poly)    ? ModSrcMode::Poly
                                        : ModSrcMode::Mono;
    s.bipolar = bipolar;
    return s;
}

void ModMatrix::setSourceMode(uint16_t srcIdx, ModSrcMode mode) {
    ASSERT(srcIdx < src_count_, "Source index out of bounds");
    ASSERT(src_registry_[srcIdx].type == ModSrcType::Both,
           "setSourceMode only valid for sources with ModSrcType::Both");
    src_registry_[srcIdx].mode = mode;
    recompileProgram();
}

ModDestination& ModMatrix::registerDestination(const std::string& string_id, ModDstMode mode,
                                               applause::ValueScaleInfo scale_info) {
    ASSERT(dst_count_ < config_.max_destinations, "max_destinations exceeded");
    ASSERT(!dst_lookup_.contains(string_id), "Destination name already registered");

    const auto idx = static_cast<uint16_t>(dst_count_++);
    dst_lookup_.emplace(string_id, idx);

    ModDestination& d = dst_registry_[idx];
    d.name = string_id;
    d.index = idx;
    d.mode = mode;
    dst_scale_info_[idx] = scale_info;

    if (mode == ModDstMode::Poly) {
        poly_dst_indices_.push_back(idx);
    }

    return d;
}

void ModMatrix::registerFromParamsExtension(const applause::ParamsExtension& params_extension) {
    ASSERT(dst_count_ == 0,
           "Cannot batch register parameters from extension after manually registering destinations");
    const auto* scale_array = params_extension.getScaleInfoArray();
    const auto& params = params_extension.getAllParameters();

    for (uint32_t i = 0; i < params.size(); ++i) {
        const auto& param = params[i];
        ModDstMode mode = param.polyphonic ? ModDstMode::Poly : ModDstMode::Mono;
        registerDestination(param.stringId, mode, scale_array[i]);
    }
}

ModConnection& ModMatrix::addConnection(ModSource src, ModDestination dst, float depth,
                                        std::optional<bool> bipolar_mapping) {
    ASSERT(src.index < src_count_, "Source index out of bounds");
    ASSERT(dst.index < dst_count_, "Destination index out of bounds");
    // Default to source's bipolar flag if not specified
    const bool mapping = bipolar_mapping.value_or(src_registry_[src.index].bipolar);

    // Check for existing parameter connection with same (src, dst)
    for (auto& existing : connections_) {
        if (!existing.isDepthMod() && existing.src_idx == src.index && existing.dst_idx == dst.index) {
            program_.depth_base_[existing.depth_slot] = depth;
            existing.flags = mapping ? (existing.flags | 0x02) : (existing.flags & ~0x02);
            recompileProgram();
            return existing;
        }
    }

    // No existing connection found; create new one
    ModConnection connection{};
    connection.matrix_ = this;
    connection.src_idx = src.index;
    connection.dst_idx = dst.index;
    connection.flags = mapping ? 0x02 : 0x00;  // bit 1 = bipolar_mapping
    connection.depth_slot = allocateDepthSlot(depth);
    connections_.push_back(connection);

    recompileProgram();
    return connections_.back();
}

bool ModMatrix::removeConnection(uint16_t srcIdx, uint16_t dstIdx) {
    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        if (!it->isDepthMod() && it->src_idx == srcIdx && it->dst_idx == dstIdx) {
            // Mark depth slot as inactive
            program_.depth_active_[it->depth_slot] = 0;
            connections_.erase(it);
            recompileProgram();
            return true;
        }
    }
    return false;
}

bool ModMatrix::removeConnection(const ModConnection& connection) {
    for (auto it = connections_.begin(); it != connections_.end(); ++it) {
        if (it->depth_slot == connection.depth_slot) {
            program_.depth_active_[it->depth_slot] = 0;
            connections_.erase(it);
            recompileProgram();
            return true;
        }
    }
    return false;
}

ModSource* ModMatrix::findSource(const std::string& name) {
    auto it = src_lookup_.find(name);
    if (it == src_lookup_.end()) return nullptr;
    return &src_registry_[it->second];
}

const ModSource* ModMatrix::findSource(const std::string& name) const {
    auto it = src_lookup_.find(name);
    if (it == src_lookup_.end()) return nullptr;
    return &src_registry_[it->second];
}

ModDestination* ModMatrix::findDestination(const std::string& name) {
    auto it = dst_lookup_.find(name);
    if (it == dst_lookup_.end()) return nullptr;
    return &dst_registry_[it->second];
}

const ModDestination* ModMatrix::findDestination(const std::string& name) const {
    auto it = dst_lookup_.find(name);
    if (it == dst_lookup_.end()) return nullptr;
    return &dst_registry_[it->second];
}

ModConnection& ModMatrix::addDepthModulation(ModSource src, const ModConnection& target_conn, float depth,
                                             std::optional<bool> bipolar_mapping) {
    ASSERT(src.index < src_count_, "Source index out of bounds");
    ASSERT(target_conn.depth_slot < program_.depth_base_.size(), "Invalid target connection");
    ASSERT(!target_conn.isDepthMod(), "Cannot modulate the depth of a depth connection (depth-1 limit)");

    const bool mapping = bipolar_mapping.value_or(src_registry_[src.index].bipolar);
    const uint16_t target_slot = target_conn.depth_slot;

    // Depth mod connections now allocate a depth slot for their own depth, even though it is impossible for a
    // modulation connection to point to this slot.
    // This allows us to use the same logic & structures for depth connections as normal connections
    // and future-proofs the interface somewhat for when/if we implement a full mod graph system
    ModConnection connection{};
    connection.matrix_ = this;
    connection.src_idx = src.index;
    connection.dst_idx = target_slot;
    connection.flags = 0x01 | (mapping ? 0x02 : 0x00);  // bit 0 = is_depth_mod, bit 1 = bipolar_mapping
    connection.depth_slot = allocateDepthSlot(depth);
    connections_.push_back(connection);

    recompileProgram();
    return connections_.back();
}

void ModMatrix::notifyVoiceOn(uint16_t voice_index) {
    ASSERT(voice_index < config_.num_voices, "voice_index out of bounds");
    if (std::ranges::find(active_voices_, voice_index) == active_voices_.end()) {
        active_voices_.push_back(voice_index);
    }
}

void ModMatrix::setBaseValue(uint16_t dstIdx, float plain_value) {
    ASSERT(dstIdx < dst_count_, "Destination index out of bounds");
    const auto& s = dst_scale_info_[dstIdx];
    float norm = s.scaling.toNormalized(plain_value, s.min, s.max);
    base_mono_dst_[dstIdx] = norm;
    base_poly_dst_[dstIdx] = norm;
}

void ModMatrix::loadParamBaseValues(const applause::ParamsExtension& params) {
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

void ModMatrix::process() {
    // Reset mono destinations to base values
    mono_dst_ = base_mono_dst_;

    // Reset poly destinations for active voices (only poly destinations need per-voice reset)
    for (size_t i = 0; i < active_voices_.size(); i++) {
        const uint16_t voice_index = active_voices_[i];
        const size_t voice_offset = static_cast<size_t>(voice_index) * poly_dst_stride_;
        for (uint16_t poly_idx : poly_dst_indices_) {
            poly_dst_buf_[voice_offset + poly_idx] = base_poly_dst_[poly_idx];
        }
    }

    // load base depth values into the mono depth buffer
    for (size_t slot = 0; slot < program_.depth_base_.size(); ++slot) {
        mono_depth_buf_[slot] = program_.depth_active_[slot] ? program_.depth_base_[slot] : 0.0f;
    }

    // calculate mono depth modulation
    for (const auto& mono_depth_conn : program_.depth_connections_mono_) {
        float src_val = mono_src_buf_[mono_depth_conn.src];
        if (mono_depth_conn.isSourceBipolar()) {
            src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
        }
        if (mono_depth_conn.isBipolar()) {
            src_val = src_val * 2.0f - 1.0f;  // [0,1] -> [-1,+1]
        }

        const float depth = program_.depth_base_[mono_depth_conn.depth_slot];
        mono_depth_buf_[mono_depth_conn.target] += src_val * depth;
    }

    // load poly depth from mono_depth_buf_
    for (uint16_t i = 0; i < active_voices_.size(); i++) {
        const uint16_t voice_index = active_voices_[i];
        for (uint16_t j = 0; j < program_.depth_base_.size(); j++) {
            poly_depth_buf_[static_cast<size_t>(voice_index) * poly_depth_stride_ + j] = mono_depth_buf_[j];
        }
    }

    // load poly depth modulation into the poly depth buffer
    for (uint16_t i = 0; i < active_voices_.size(); i++) {
        uint16_t voice_index = active_voices_[i];
        for (const auto& poly_depth_conn : program_.depth_connections_poly_) {
            float src_val =
                poly_src_buf_[static_cast<size_t>(voice_index) * poly_src_stride_ + poly_depth_conn.src];
            if (poly_depth_conn.isSourceBipolar()) {
                src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
            }
            if (poly_depth_conn.isBipolar()) {
                src_val = src_val * 2.0f - 1.0f;  // [0,1] -> [-1,+1]
            }
            const float depth = program_.depth_base_[poly_depth_conn.depth_slot];
            poly_depth_buf_[static_cast<size_t>(voice_index) * poly_depth_stride_ + poly_depth_conn.target] +=
                src_val * depth;
        }
    }

    // now that the modulation depth pass is done, we can calculate final modulation values
    // first, mono -> mono connections
    // NOTE: MM connections read from mono_depth_buf_, so poly depth modulation on these
    // depth slots is silently ignored. This is analogous to PM connections (NYI) - both
    // require a reduction policy to collapse per-voice values to mono. Until implemented,
    // avoid poly depth mods on slots used by MM connections.
    for (const auto& mm_conn : program_.mm_connections) {
        float src_val = mono_src_buf_[mm_conn.src];
        if (mm_conn.isSourceBipolar()) {
            src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
        }
        if (mm_conn.isBipolar()) {
            src_val = src_val * 2.0f - 1.0f;  // [0,1] -> [-1,+1]
        }
        const float depth_val = mono_depth_buf_[mm_conn.depth_slot];
        mono_dst_[mm_conn.target] += src_val * depth_val;
    }

    // mono -> poly connections
    for (uint16_t i = 0; i < active_voices_.size(); i++) {
        uint16_t voice_index = active_voices_[i];
        for (const auto& mp_conn : program_.mp_connections) {
            float src_val = mono_src_buf_[mp_conn.src];
            if (mp_conn.isSourceBipolar()) {
                src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
            }
            if (mp_conn.isBipolar()) {
                src_val = src_val * 2.0f - 1.0f;  // [0,1] -> [-1,+1]
            }
            const float depth_val =
                poly_depth_buf_[static_cast<size_t>(voice_index) * poly_depth_stride_ + mp_conn.depth_slot];
            poly_dst_buf_[static_cast<size_t>(voice_index) * poly_dst_stride_ + mp_conn.target] +=
                src_val * depth_val;
        }
    }

    // poly -> poly connections
    for (uint16_t i = 0; i < active_voices_.size(); i++) {
        uint16_t voice_index = active_voices_[i];
        for (const auto& pp_conn : program_.pp_connections) {
            float src_val = poly_src_buf_[static_cast<size_t>(voice_index) * poly_src_stride_ + pp_conn.src];
            if (pp_conn.isSourceBipolar()) {
                src_val = (src_val + 1.0f) * 0.5f;  // [-1,+1] -> [0,1]
            }
            if (pp_conn.isBipolar()) {
                src_val = src_val * 2.0f - 1.0f;  // [0,1] -> [-1,+1]
            }
            const float depth_val =
                poly_depth_buf_[static_cast<size_t>(voice_index) * poly_depth_stride_ + pp_conn.depth_slot];
            poly_dst_buf_[static_cast<size_t>(voice_index) * poly_dst_stride_ + pp_conn.target] +=
                src_val * depth_val;
        }
    }

    // poly -> mono connections (NYI -- leave as stub for now...)


    // Scale mono destinations: normalized -> true-value
    for (uint16_t i = 0; i < dst_count_; i++) {
        const auto& s = dst_scale_info_[i];
        float norm = std::clamp(mono_dst_[i], 0.0f, 1.0f);
        mono_dst_[i] = s.scaling.fromNormalized(norm, s.min, s.max);
    }

    // Scale poly destinations for active voices: normalized -> true-value
    // Only iterate over poly destinations; mono destinations don't need per-voice scaling
    for (const auto voice_index : active_voices_) {
        const size_t voice_offset = static_cast<size_t>(voice_index) * poly_dst_stride_;

        for (uint16_t poly_idx : poly_dst_indices_) {
            const auto& s = dst_scale_info_[poly_idx];
            float norm = std::clamp(poly_dst_buf_[voice_offset + poly_idx], 0.0f, 1.0f);
            poly_dst_buf_[voice_offset + poly_idx] = s.scaling.fromNormalized(norm, s.min, s.max);
        }
    }
}

uint16_t ModMatrix::allocateDepthSlot(float initial_depth) {
    for (size_t i = 0; i < program_.depth_active_.size(); ++i) {
        if (program_.depth_active_[i] == 0) {
            program_.depth_base_[i] = initial_depth;
            program_.depth_active_[i] = 1;
            return static_cast<uint16_t>(i);
        }
    }
    // No holes found, append
    ASSERT(program_.depth_base_.size() < config_.max_connections, "max_connections exceeded");
    program_.depth_base_.push_back(initial_depth);
    program_.depth_active_.push_back(1);
    return static_cast<uint16_t>(program_.depth_base_.size() - 1);
}

void ModMatrix::recompileProgram() {
    program_.mm_connections.clear();
    program_.mp_connections.clear();
    program_.pm_connections.clear();
    program_.pp_connections.clear();
    program_.depth_connections_mono_.clear();
    program_.depth_connections_poly_.clear();

    for (const auto& conn : connections_) {
        const auto& src = src_registry_[conn.src_idx];
        const bool src_bipolar = src.bipolar;
        const ModSrcMode src_mode = (src.type == ModSrcType::Mono) ? ModSrcMode::Mono
            : (src.type == ModSrcType::Poly)                       ? ModSrcMode::Poly
                                                                   : src.mode;

        // Build flags: bit 0 = is_depth_mod, bit 1 = src_bipolar, bit 2 = bipolar_mapping
        uint8_t handle_flags = 0;
        if (conn.isDepthMod()) handle_flags |= 0x01;
        if (src_bipolar) handle_flags |= 0x02;
        if (conn.isBipolar()) handle_flags |= 0x04;

        ModConnectionHandle handle{conn.src_idx, conn.dst_idx, conn.depth_slot, handle_flags};

        if (conn.isDepthMod()) {
            // Depth modulation connection - partition by source mono/poly
            if (src_mode == ModSrcMode::Mono) {
                program_.depth_connections_mono_.push_back(handle);
            } else {
                program_.depth_connections_poly_.push_back(handle);
            }
        } else {
            // Parameter connection - partition by src/dst mono/poly routing
            const ModDstMode dst_mode = dst_registry_[conn.dst_idx].mode;

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
}

// ModConnection implementations (require complete ModMatrix definition)

float ModConnection::getDepth() const { return matrix_->getDepthBase(depth_slot); }

void ModConnection::setDepth(float d) { matrix_->setDepthBase(depth_slot, d); }

void ModConnection::setBipolar(bool v) {
    if (isBipolar() != v) {
        flags = v ? (flags | 0x02) : (flags & ~0x02);
        matrix_->recompileProgram();
    }
}

const ModSource& ModConnection::source() const { return matrix_->getSource(src_idx); }

const ModDestination* ModConnection::destination() const {
    return isDepthMod() ? nullptr : &matrix_->getDestination(dst_idx);
}

}  // namespace applause
