#pragma once

#include "BatchedVoice.h"
#include "SimdVoiceProxy.h"

#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>

#include <xsimd/xsimd.hpp>

// Maps scalar voice proxies to fixed SIMD batches and lanes.
class SimdVoiceManager final {
public:
    static constexpr std::size_t voice_count = 16;
    static constexpr std::size_t lane_count = BatchedVoice::lane_count;
    static constexpr std::size_t batch_count = voice_count / lane_count;

    static_assert(voice_count % lane_count == 0);
    static_assert(batch_count == 4);

    SimdVoiceManager() = default;
    SimdVoiceManager(const SimdVoiceManager&) = delete;
    SimdVoiceManager(SimdVoiceManager&&) = delete;
    SimdVoiceManager& operator=(const SimdVoiceManager&) = delete;
    SimdVoiceManager& operator=(SimdVoiceManager&&) = delete;

    void bind(std::span<SimdVoiceProxy> proxies) noexcept {
        assert(proxy_base_ == nullptr);
        assert(proxies.size() == voice_count);

        proxy_base_ = proxies.data();
        for (std::size_t slot = 0; slot < voice_count; ++slot) proxy_base_[slot].bind(*this, slot);
    }

    void bindParameters(BatchedVoice::ModMatrix& matrix) {
        for (std::size_t batch = 0; batch < batch_count; ++batch)
            batches_[batch].bindParameters(matrix, static_cast<std::uint16_t>(batch));
    }

    void preProcess(std::size_t slot, const applause::Note& note) noexcept {
        assert(slot < voice_count);
        batches_[batchForSlot(slot)].preProcess(laneMask(laneForSlot(slot)), note);
    }

    void start(std::size_t slot, const applause::Note& note) noexcept {
        assert(slot < voice_count);
        assert(proxy_base_ != nullptr);
        batches_[batchForSlot(slot)].start(laneMask(laneForSlot(slot)), note);
    }

    void release(std::size_t slot) noexcept {
        assert(slot < voice_count);
        auto& batch = batches_[batchForSlot(slot)];
        if (xsimd::any(batch.release(laneMask(laneForSlot(slot)))))
            proxy_base_[slot].terminateVoice();
    }

    void kill(std::size_t slot) noexcept {
        assert(slot < voice_count);
        assert(proxy_base_ != nullptr);

        auto& batch = batches_[batchForSlot(slot)];
        const auto lane = laneMask(laneForSlot(slot));
        batch.kill(lane);
        proxy_base_[slot].terminateVoice();
    }

    void reset() noexcept {
        for (std::size_t slot = 0; slot < voice_count; ++slot) kill(slot);
    }

    void expressionChanged(std::size_t slot, applause::Note::Expression expression,
                           const applause::Note& note) noexcept {
        assert(slot < voice_count);
        batches_[batchForSlot(slot)].expressionChanged(laneMask(laneForSlot(slot)), expression, note);
    }

    [[nodiscard]] auto& batches() noexcept { return batches_; }
    [[nodiscard]] const auto& batches() const noexcept { return batches_; }

    [[nodiscard]] bool empty() const noexcept {
        for (const auto& batch : batches_) {
            if (!batch.empty()) return false;
        }
        return true;
    }

    void render(applause::BufferView<float> output, int start_sample, int num_samples) noexcept {
        for (std::size_t batch = 0; batch < batch_count; ++batch) {
            if (batches_[batch].empty()) continue;
            auto finished = batches_[batch].renderAdd(output, start_sample, num_samples).mask();
            while (finished != 0) {
                const auto lane = static_cast<std::size_t>(std::countr_zero(finished));
                proxy_base_[batch * lane_count + lane].terminateVoice();
                finished &= finished - 1;
            }
        }
    }

private:
    [[nodiscard]] static constexpr std::size_t batchForSlot(std::size_t slot) noexcept {
        return slot / lane_count;
    }

    [[nodiscard]] static constexpr std::size_t laneForSlot(std::size_t slot) noexcept {
        return slot % lane_count;
    }

    [[nodiscard]] static BatchedVoice::Mask laneMask(std::size_t lane) noexcept {
        assert(lane < lane_count);
        return BatchedVoice::Mask::from_mask(std::uint64_t{1} << lane);
    }

    std::array<BatchedVoice, batch_count> batches_{};
    SimdVoiceProxy* proxy_base_ = nullptr;
};

inline void SimdVoiceProxy::bind(SimdVoiceManager& manager, std::size_t logical_slot) noexcept {
    assert(manager_ == nullptr);
    assert(logical_slot < SimdVoiceManager::voice_count);
    manager_ = &manager;
    logical_slot_ = logical_slot;
}

inline void SimdVoiceProxy::onPreProcess() noexcept {
    assert(isBound());
    manager_->preProcess(logical_slot_, note_);
}

inline void SimdVoiceProxy::noteOn() noexcept {
    assert(isBound());
    manager_->start(logical_slot_, note_);
}

inline void SimdVoiceProxy::noteOff(bool terminate_now) noexcept {
    assert(isBound());
    if (terminate_now)
        manager_->kill(logical_slot_);
    else
        manager_->release(logical_slot_);
}

inline void SimdVoiceProxy::onExpressionChange(applause::Note::Expression expression, double) noexcept {
    assert(isBound());
    manager_->expressionChanged(logical_slot_, expression, note_);
}
