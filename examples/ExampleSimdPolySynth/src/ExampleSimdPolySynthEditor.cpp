#include "ExampleSimdPolySynthEditor.h"

#include <algorithm>

namespace {

constexpr float kOuterPadding = 12.0f;
constexpr float kPanelGap = 10.0f;
constexpr float kControlGap = 10.0f;
constexpr float kPanelInset = 12.0f;

constexpr float kKnobWidth = 56.0f;
constexpr float kKnobLabelHeight = 20.0f;
constexpr float kWaveformRowHeight = 38.0f;
constexpr float kOscInset = 14.0f;
constexpr float kOscUpperGap = 36.0f;
constexpr float kOscLowerGap = 18.0f;
constexpr float kFilterKnobGap = 24.0f;
constexpr float kEnvelopeKnobGap = 28.0f;

}  // namespace

ExampleSimdPolySynthEditor::ExampleSimdPolySynthEditor(
    applause::ParamsExtension* params, applause::ModMatrixControl* mod_matrix) :
    applause::ApplauseEditor(params) {
    setFixedAspectRatio(true);

    addChild(&osc1_panel_);
    addChild(&osc2_panel_);
    addChild(&filter_panel_);
    addChild(&envelope_panel_);
    addChild(&mod_matrix_panel_);

    filter_curve_.points[0] = {0.03f, 0.92f};
    filter_curve_.points[1] = {0.48f, 0.90f};
    filter_curve_.points[2] = {0.68f, 0.68f};
    filter_curve_.points[3] = {0.82f, 0.25f};
    filter_curve_.points[4] = {0.97f, 0.06f};
    filter_curve_.curvature_power[1] = 2.0f;
    filter_curve_.curvature_power[2] = 3.5f;
    filter_curve_.num_points = 5;
    filter_curve_.loop = false;

    envelope_curve_.points[0] = {0.03f, 0.05f};
    envelope_curve_.points[1] = {0.17f, 0.95f};
    envelope_curve_.points[2] = {0.36f, 0.68f};
    envelope_curve_.points[3] = {0.72f, 0.68f};
    envelope_curve_.points[4] = {0.97f, 0.05f};
    envelope_curve_.curvature_power[0] = -2.5f;
    envelope_curve_.curvature_power[1] = 2.0f;
    envelope_curve_.curvature_power[3] = 3.0f;
    envelope_curve_.num_points = 5;
    envelope_curve_.loop = false;

    filter_display_.setCurve(&filter_curve_);
    envelope_display_.setCurve(&envelope_curve_);
    filter_display_.allowAddRemovePoints(false);
    envelope_display_.allowAddRemovePoints(false);
    filter_display_.setIgnoresMouseEvents(true, true);
    envelope_display_.setIgnoresMouseEvents(true, true);
    filter_plot_.addChild(&filter_display_);
    envelope_plot_.addChild(&envelope_display_);
    filter_panel_.content().addChild(&filter_plot_);
    envelope_panel_.content().addChild(&envelope_plot_);

    if (params) {
        auto makeKnob = [params, mod_matrix](const char* id) {
            const auto* destination = mod_matrix ? mod_matrix->findDestination(id) : nullptr;
            return std::make_unique<applause::ParamKnob>(params->getInfo(id), destination);
        };

        osc1_waveform_grid_ = std::make_unique<applause::ParamSelectionGrid>(
            params->getInfo("osc1_waveform"), 4, 1);
        osc2_waveform_grid_ = std::make_unique<applause::ParamSelectionGrid>(
            params->getInfo("osc2_waveform"), 4, 1);
        osc1_panel_.content().addChild(osc1_waveform_grid_.get());
        osc2_panel_.content().addChild(osc2_waveform_grid_.get());

        constexpr std::array osc1_ids{"osc1_octave", "osc1_semitone", "osc1_fine", "osc1_level",
                                      "osc1_pan"};
        constexpr std::array osc2_ids{"osc2_octave", "osc2_semitone", "osc2_fine", "osc2_level",
                                      "osc2_pan"};
        for (std::size_t i = 0; i < osc1_knobs_.size(); ++i) {
            osc1_knobs_[i] = makeKnob(osc1_ids[i]);
            osc2_knobs_[i] = makeKnob(osc2_ids[i]);
            osc1_panel_.content().addChild(osc1_knobs_[i].get());
            osc2_panel_.content().addChild(osc2_knobs_[i].get());
        }

        filter_type_grid_ = std::make_unique<applause::ParamSelectionGrid>(
            params->getInfo("filter_type"), 3, 1);
        filter_panel_.content().addChild(filter_type_grid_.get());

        constexpr std::array filter_ids{"filter_cutoff", "filter_resonance", "filter_drive",
                                        "filter_env_amount"};
        for (std::size_t i = 0; i < filter_knobs_.size(); ++i) {
            filter_knobs_[i] = makeKnob(filter_ids[i]);
            filter_panel_.content().addChild(filter_knobs_[i].get());
        }

        constexpr std::array envelope_ids{"attack", "decay", "sustain", "release"};
        for (std::size_t i = 0; i < envelope_knobs_.size(); ++i) {
            envelope_knobs_[i] = makeKnob(envelope_ids[i]);
            envelope_panel_.content().addChild(envelope_knobs_[i].get());
        }
    }

    if (mod_matrix) {
        mod_matrix_ui_ = std::make_unique<applause::ModMatrixComponent>(*mod_matrix);
        mod_matrix_panel_.content().addChild(mod_matrix_ui_.get());
    }
}

void ExampleSimdPolySynthEditor::resized() {
    const float available_width = std::max(0.0f, width() - 2.0f * kOuterPadding);
    const float available_height = std::max(0.0f, height() - 2.0f * kOuterPadding);
    const float top_height = available_height * 0.58f;
    const float matrix_height = std::max(0.0f, available_height - top_height - kPanelGap);
    const float top_panels_width = std::max(0.0f, available_width - 3.0f * kPanelGap);
    const float oscillator_width = top_panels_width * 0.20f;
    const float filter_width = top_panels_width * 0.28f;
    const float envelope_width = top_panels_width - 2.0f * oscillator_width - filter_width;

    float x = kOuterPadding;
    osc1_panel_.setBounds(x, kOuterPadding, oscillator_width, top_height);
    x += oscillator_width + kPanelGap;
    osc2_panel_.setBounds(x, kOuterPadding, oscillator_width, top_height);
    x += oscillator_width + kPanelGap;
    filter_panel_.setBounds(x, kOuterPadding, filter_width, top_height);
    x += filter_width + kPanelGap;
    envelope_panel_.setBounds(x, kOuterPadding, envelope_width, top_height);

    const float matrix_y = kOuterPadding + top_height + kPanelGap;
    mod_matrix_panel_.setBounds(kOuterPadding, matrix_y, available_width, matrix_height);

    const float osc_inner_width = std::max(0.0f, osc1_panel_.content().width() - 2.0f * kOscInset);
    const float filter_inner_width =
        std::max(0.0f, filter_panel_.content().width() - 2.0f * kPanelInset);
    const float envelope_inner_width =
        std::max(0.0f, envelope_panel_.content().width() - 2.0f * kPanelInset);

    // Every knob shares one size, so the tightest row sets it for all of them.
    const float knob_width = std::max(0.0f, std::min({kKnobWidth,
        (osc_inner_width - kOscUpperGap) * 0.5f,
        (osc_inner_width - 2.0f * kOscLowerGap) / 3.0f,
        (filter_inner_width - 3.0f * kFilterKnobGap) / 4.0f,
        (envelope_inner_width - 3.0f * kEnvelopeKnobGap) / 4.0f}));
    const float knob_height = knob_width + kKnobLabelHeight;

    auto layoutOscillator = [knob_width, knob_height](applause::Frame& content,
                                                      applause::ParamSelectionGrid* waveform,
                                                      auto& knobs) {
        constexpr float kSectionGap = 18.0f;
        constexpr float kRowGap = 14.0f;
        const float inner_width = std::max(0.0f, content.width() - 2.0f * kOscInset);
        const float upper_row_width = 2.0f * knob_width + kOscUpperGap;
        const float lower_row_width = 3.0f * knob_width + 2.0f * kOscLowerGap;
        const float waveform_width = std::min(inner_width, lower_row_width);
        const float waveform_height = std::min(kWaveformRowHeight, content.height() * 0.2f);
        const float stack_height = waveform_height + kSectionGap + 2.0f * knob_height + kRowGap;
        float y = std::max(kOscInset, (content.height() - stack_height) * 0.5f);

        if (waveform) {
            waveform->setBounds((content.width() - waveform_width) * 0.5f, y,
                                waveform_width, waveform_height);
        }
        y += waveform_height + kSectionGap;

        const float upper_x = (content.width() - upper_row_width) * 0.5f;
        for (std::size_t i = 0; i < 2; ++i) {
            if (knobs[i])
                knobs[i]->setBounds(upper_x + i * (knob_width + kOscUpperGap), y,
                                    knob_width, knob_height);
        }
        y += knob_height + kRowGap;

        const float lower_x = (content.width() - lower_row_width) * 0.5f;
        for (std::size_t i = 0; i < 3; ++i) {
            if (knobs[i + 2])
                knobs[i + 2]->setBounds(lower_x + i * (knob_width + kOscLowerGap), y,
                                        knob_width, knob_height);
        }
    };
    layoutOscillator(osc1_panel_.content(), osc1_waveform_grid_.get(), osc1_knobs_);
    layoutOscillator(osc2_panel_.content(), osc2_waveform_grid_.get(), osc2_knobs_);

    auto& filter_content = filter_panel_.content();
    const float filter_inner_height = std::max(0.0f, filter_content.height() - 2.0f * kPanelInset);
    const float filter_plot_height = std::min(154.0f, filter_inner_height * 0.50f);
    const float filter_stack_height =
        filter_plot_height + kControlGap + 36.0f + 16.0f + knob_height;
    const float filter_top = std::max(kPanelInset,
        (filter_content.height() - filter_stack_height) * 0.5f);
    filter_plot_.setBounds(kPanelInset, filter_top, filter_inner_width, filter_plot_height);
    filter_display_.setBounds(kControlGap, kControlGap,
                              std::max(0.0f, filter_plot_.width() - 2.0f * kControlGap),
                              std::max(0.0f, filter_plot_.height() - 2.0f * kControlGap));

    constexpr float kFilterGridHeight = 36.0f;
    const float filter_grid_width = std::min(filter_inner_width, 238.0f);
    const float filter_grid_y = filter_top + filter_plot_height + kControlGap;
    if (filter_type_grid_)
        filter_type_grid_->setBounds((filter_content.width() - filter_grid_width) * 0.5f,
                                     filter_grid_y, filter_grid_width, kFilterGridHeight);

    const float filter_row_width = 4.0f * knob_width + 3.0f * kFilterKnobGap;
    const float filter_knob_x = (filter_content.width() - filter_row_width) * 0.5f;
    const float filter_knob_y = filter_grid_y + kFilterGridHeight + 16.0f;
    for (std::size_t i = 0; i < filter_knobs_.size(); ++i) {
        if (filter_knobs_[i])
            filter_knobs_[i]->setBounds(filter_knob_x + i * (knob_width + kFilterKnobGap),
                                        filter_knob_y, knob_width, knob_height);
    }

    auto& envelope_content = envelope_panel_.content();
    const float envelope_inner_height =
        std::max(0.0f, envelope_content.height() - 2.0f * kPanelInset);
    const float envelope_plot_height = std::min(186.0f, envelope_inner_height * 0.60f);
    constexpr float kEnvelopeRowGap = 18.0f;
    const float envelope_stack_height = envelope_plot_height + kEnvelopeRowGap + knob_height;
    const float envelope_top = std::max(kPanelInset,
        (envelope_content.height() - envelope_stack_height) * 0.5f);
    envelope_plot_.setBounds(kPanelInset, envelope_top, envelope_inner_width, envelope_plot_height);
    envelope_display_.setBounds(kControlGap, kControlGap,
                                std::max(0.0f, envelope_plot_.width() - 2.0f * kControlGap),
                                std::max(0.0f, envelope_plot_.height() - 2.0f * kControlGap));

    const float envelope_row_width = 4.0f * knob_width + 3.0f * kEnvelopeKnobGap;
    const float envelope_knob_x = (envelope_content.width() - envelope_row_width) * 0.5f;
    const float envelope_knob_y = envelope_top + envelope_plot_height + kEnvelopeRowGap;
    for (std::size_t i = 0; i < envelope_knobs_.size(); ++i) {
        if (envelope_knobs_[i])
            envelope_knobs_[i]->setBounds(envelope_knob_x + i * (knob_width + kEnvelopeKnobGap),
                                          envelope_knob_y, knob_width, knob_height);
    }

    if (mod_matrix_ui_) {
        auto& matrix_content = mod_matrix_panel_.content();
        mod_matrix_ui_->setBounds(0.0f, 0.0f, matrix_content.width(), matrix_content.height());
    }
}
