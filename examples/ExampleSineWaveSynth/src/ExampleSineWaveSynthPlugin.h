#pragma once

#include <applause/core/PluginBase.h>
#include <applause/dsp/Synthesizer.h>
#include <applause/extensions/AudioPortsExtension.h>
#include <applause/extensions/NotePortsExtension.h>
#include <applause/extensions/StateExtension.h>

#include <cmath>

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

class SineWaveVoice : public applause::SynthesizerVoice<float, 2> {
public:
    void noteOn() override {
        const float frequency = static_cast<float>(note_.getFrequency());
        phase_increment_ = kTwoPi * frequency / static_cast<float>(sample_rate_);
        phase_ = 0.0f;
        envelope_ = 0.0f;
        release_samples_ = -1;
    }

    void noteOff(bool terminate_now) override {
        if (terminate_now) {
            terminateVoice();
        } else {
            release_samples_ = static_cast<int>(sample_rate_ * 0.01);
        }
    }

    void onExpressionChange(applause::Note::Expression expression_id, double value) override {
        // React to expression changes by recalculating cached values
        if (expression_id == applause::Note::Expression::Tuning) {
            const float frequency = static_cast<float>(note_.getFrequency());
            phase_increment_ = kTwoPi * frequency / static_cast<float>(sample_rate_);
        }
        // This simple synth ignores other expressions, but you could handle:
        // Note::Expression::Volume: adjust envelope target
        // Note::Expression::Dynamic: modulate amplitude (MIDI CC 11)
        // Note::Expression::Timbre: modulate filter cutoff (MPE Y-axis)
        // Note::Expression::Pressure: modulate amplitude or vibrato (MPE Z-axis)
    }

    void process(applause::BufferView<float, 2> buffer, int start_sample, int num_samples) override {
        const float velocity_scale = static_cast<float>(note_.note_on_velocity);
        const float attack_samples = static_cast<float>(sample_rate_ * 0.01);

        auto left_channel = buffer.channel(0);
        auto right_channel = buffer.channel(1);

        for (int i = 0; i < num_samples; ++i) {
            if (release_samples_ >= 0) {
                const float release_total =
                    static_cast<float>(sample_rate_ * 0.01);
                envelope_ = static_cast<float>(release_samples_) / release_total;
                release_samples_--;

                if (release_samples_ < 0) {
                    terminateVoice();
                    return;
                }
            } else if (envelope_ < 1.0f) {
                envelope_ += 1.0f / attack_samples;
                if (envelope_ > 1.0f) envelope_ = 1.0f;
            }

            const float sample =
                std::sin(phase_) * envelope_ * velocity_scale * 0.3f;

            const int frame_index = start_sample + i;
            left_channel.add(frame_index, sample);
            right_channel.add(frame_index, sample);

            phase_ += phase_increment_;
            if (phase_ >= kTwoPi) {
                phase_ -= kTwoPi;
            }
        }
    }

private:
    float phase_ = 0.0f;
    float phase_increment_ = 0.0f;
    float envelope_ = 0.0f;
    int release_samples_ = -1;
};

class ExampleSineWaveSynthPlugin : public applause::PluginBase {
public:
    explicit ExampleSineWaveSynthPlugin(
        const clap_plugin_descriptor_t* descriptor, const clap_host_t* host);
    ~ExampleSineWaveSynthPlugin() override = default;

    bool init() noexcept override;
    void destroy() noexcept override;
    bool activate(const applause::ProcessInfo& info) noexcept override;
    void deactivate() noexcept override;

    clap_process_status process(const clap_process_t* process) noexcept override;

private:
    applause::NotePortsExtension note_ports_;
    applause::AudioPortsExtension audio_ports_;
    applause::StateExtension state_;

    applause::Synthesizer<float, 2, 16, SineWaveVoice> synth_;
    double sample_rate_ = 44100.0;
};
