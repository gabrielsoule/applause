#pragma once

#include "SimdVoiceManager.h"

#include <applause/core/ModMatrix.h>
#include <applause/dsp/Synthesizer.h>

#include <cstddef>
#include <cstdint>

class SimdPolySynth final : public applause::Synthesizer<SimdVoiceProxy, SimdVoiceManager::voice_count> {
public:
    using Batch = BatchedVoice::Batch;
    using ModMatrix = applause::ModMatrix<Batch>;

    static constexpr std::size_t voice_count = SimdVoiceManager::voice_count;
    static constexpr std::size_t lane_count = SimdVoiceManager::lane_count;
    static constexpr std::size_t batch_count = SimdVoiceManager::batch_count;

    static_assert(Batch::size == lane_count, "ExampleSimdPolySynth requires four-lane float SIMD");
    static_assert(voice_count % lane_count == 0, "Every voice slot must belong to a full SIMD batch");

    explicit SimdPolySynth(ModMatrix& mod_matrix) :
        mod_matrix_(mod_matrix),
        envelope_source_(mod_matrix_.registerSource("Envelope", applause::ModSrcType::Poly, false).index),
        timbre_source_(mod_matrix_.registerSource("Timbre", applause::ModSrcType::Poly, false).index),
        pressure_source_(mod_matrix_.registerSource("Pressure", applause::ModSrcType::Poly, false).index) {
        manager_.bind(getVoices());
    }
    SimdPolySynth(const SimdPolySynth&) = delete;
    SimdPolySynth(SimdPolySynth&&) = delete;
    SimdPolySynth& operator=(const SimdPolySynth&) = delete;
    SimdPolySynth& operator=(SimdPolySynth&&) = delete;

    [[nodiscard]] bool empty() const noexcept { return manager_.empty(); }
    void bindParameters() { manager_.bindParameters(mod_matrix_); }
    void activate(applause::ProcessInfo info) noexcept {
        applause::Synthesizer<SimdVoiceProxy, voice_count>::activate(info);
        for (auto& batch : manager_.batches()) batch.activate(info.sample_rate);
    }
    void process(BufferType buffer, const clap_input_events_t* events) {
        applause::Synthesizer<SimdVoiceProxy, voice_count>::process(buffer, events);
    }
    void reset() noexcept {
        manager_.reset();
        syncModMatrixVoiceMasks();
    }

private:
    void onPreProcess() noexcept override {
        syncModMatrixVoiceMasks();
        const auto& batches = manager_.batches();
        for (std::size_t batch = 0; batch < batch_count; ++batch) {
            mod_matrix_.setPolySourceValue(envelope_source_, static_cast<std::uint16_t>(batch),
                                           batches[batch].envelopeValue());
            mod_matrix_.setPolySourceValue(timbre_source_, static_cast<std::uint16_t>(batch),
                                           batches[batch].timbreValue());
            mod_matrix_.setPolySourceValue(pressure_source_, static_cast<std::uint16_t>(batch),
                                           batches[batch].pressureValue());
        }
        mod_matrix_.process();
    }

    void renderSubBlock(BufferType buffer, int start_sample, int num_samples) noexcept override {
        if (manager_.empty()) return;
        manager_.render(buffer, start_sample, num_samples);
    }

    // helper function to tell the SIMD mod matrix which lanes (read: logical voices) are active
    void syncModMatrixVoiceMasks() noexcept {
        const auto voices = getVoices();
        for (std::size_t batch = 0; batch < batch_count; ++batch) {
            BatchedVoice::Mask mask{false};
            for (std::size_t lane = 0; lane < lane_count; ++lane) {
                if (voices[batch * lane_count + lane].active_)
                    mask |= BatchedVoice::Mask::from_mask(std::uint64_t{1} << lane);
            }
            mod_matrix_.setActiveMask(static_cast<std::uint16_t>(batch), mask);
        }
    }

    SimdVoiceManager manager_;
    ModMatrix& mod_matrix_;
    std::uint16_t envelope_source_;
    std::uint16_t timbre_source_;
    std::uint16_t pressure_source_;
};
