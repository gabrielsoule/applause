#pragma once

#include <applause/dsp/Synthesizer.h>

#include <cassert>
#include <cstddef>

class SimdVoiceManager;

// A proxy represents one logical voice. It does not render audio.
class SimdVoiceProxy final : public applause::SynthesizerVoice<float> {
public:
    SimdVoiceProxy() = default;
    SimdVoiceProxy(const SimdVoiceProxy&) = delete;
    SimdVoiceProxy(SimdVoiceProxy&&) = delete;
    SimdVoiceProxy& operator=(const SimdVoiceProxy&) = delete;
    SimdVoiceProxy& operator=(SimdVoiceProxy&&) = delete;

    [[nodiscard]] bool isBound() const noexcept { return manager_ != nullptr; }

    [[nodiscard]] std::size_t logicalSlot() const noexcept {
        assert(isBound());
        return logical_slot_;
    }

    void process(BufferType, int, int) noexcept override {
        assert(false && "SimdVoiceProxy must not render independently");
    }

    void onPreProcess() noexcept override;
    void noteOn() noexcept override;
    void noteOff(bool) noexcept override;
    void onExpressionChange(applause::Note::Expression, double) noexcept override;

private:
    friend class SimdVoiceManager;

    void bind(SimdVoiceManager& manager, std::size_t logical_slot) noexcept;

    SimdVoiceManager* manager_ = nullptr;
    std::size_t logical_slot_ = 0;
};
