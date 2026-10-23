#include "ExampleShowcaseEditor.h"

#include <applause/ui/ApplauseUI.h>
#ifdef __APPLE__
#include <applause/ui/NativePopupMenu.h>
#endif
#include <applause/ui/Tooltip.h>
#include <applause/util/DebugHelpers.h>
#ifndef NDEBUG
#include <applause/util/inspector/InspectorWindow.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <nfd.hpp>
#include <numbers>
#include <span>
#include <sstream>
#include <string_view>
#include <tuple>
#include <utility>

using namespace applause::dimension;

namespace {

// Inline icons, so the buttons demo needs no resource files.
constexpr std::string_view kFolderIcon =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path d="M2 5 L9 5 L11 7 L22 7 L22 20 L2 20 Z"/></svg>)";
constexpr std::string_view kStarIcon =
    R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path d="M12 2 L15 9 L22 9 L16.5 13.5 L18.5 21 L12 16.5 L5.5 21 L7.5 13.5 L2 9 L9 9 Z"/></svg>)";

applause::Svg icon(std::string_view svg) {
    return {reinterpret_cast<const unsigned char*>(svg.data()), static_cast<int>(svg.size())};
}

class WaveformCell final : public applause::SelectionGridCell {
public:
    enum class Waveform { Sine, Triangle, SawUp, SawDown, Square, Pulse };

    explicit WaveformCell(Waveform waveform) : waveform_(waveform) {}

protected:
    void drawContent(applause::Canvas& canvas, float hover_amount) override {
        if (selected())
            canvas.setColor(applause::color(*this, kContentSelected)
                                .interpolateWith(applause::color(*this, kContentSelectedHover), hover_amount));
        else
            canvas.setColor(
                applause::color(*this, kContent).interpolateWith(applause::color(*this, kContentHover), hover_amount));
        const auto bounds = localBounds().reduced(applause::value(*this, kContentPadding));
        const float line_width = 2.0f;
        const float x_inset =
            std::min(line_width * 0.5f + 0.5f / canvas.dpiScale(), bounds.width() * 0.5f);
        const float left = bounds.x() + x_inset;
        const float top = bounds.y();
        const float draw_width = bounds.width() - 2.0f * x_inset;
        const float draw_height = bounds.height();
        const auto point = [&](float x, float y) {
            return applause::Point{left + x * draw_width, top + y * draw_height};
        };
        const auto polyline = [&](std::initializer_list<applause::Point> points) {
            auto previous = points.begin();
            for (auto current = std::next(previous); current != points.end(); ++current, ++previous)
                canvas.segment(previous->x, previous->y, current->x, current->y, line_width, true);
        };

        switch (waveform_) {
            case Waveform::Sine: {
                auto previous = point(0.0f, 0.5f);
                for (int i = 1; i <= 24; ++i) {
                    const float phase = static_cast<float>(i) / 24.0f;
                    auto current = point(phase, 0.5f - 0.42f * std::sin(phase * 2.0f * std::numbers::pi_v<float>));
                    canvas.segment(previous.x, previous.y, current.x, current.y, line_width, true);
                    previous = current;
                }
                break;
            }
            case Waveform::Triangle:
                polyline({point(0.0f, 0.5f), point(0.25f, 0.08f), point(0.75f, 0.92f), point(1.0f, 0.5f)});
                break;
            case Waveform::SawUp:
                polyline({point(0.0f, 0.92f), point(1.0f, 0.08f), point(1.0f, 0.92f)});
                break;
            case Waveform::SawDown:
                polyline({point(0.0f, 0.08f), point(0.0f, 0.92f), point(1.0f, 0.08f)});
                break;
            case Waveform::Square:
                polyline({point(0.0f, 0.08f), point(0.5f, 0.08f), point(0.5f, 0.92f), point(1.0f, 0.92f)});
                break;
            case Waveform::Pulse:
                polyline({point(0.0f, 0.92f), point(0.2f, 0.92f), point(0.2f, 0.08f), point(0.45f, 0.08f),
                          point(0.45f, 0.92f), point(1.0f, 0.92f)});
                break;
        }
    }

private:
    Waveform waveform_;
};

}  // namespace

ExampleShowcaseEditor::ExampleShowcaseEditor(applause::ParamsExtension* params,
                                             applause::ModMatrixControl* mod_matrix) :
    applause::ApplauseEditor(params) {
    ApplauseEditor::setFixedAspectRatio(true);

    // --- Parameters Panel (left column) ---
    addChild(&params_panel_);

    parameter_ui_ = std::make_unique<applause::GenericParameterUI>();
    if (getParamsExtension()) {
        for (auto& param : getParamsExtension()->getAllParameters()) {
            if (!param.internal) {
                parameter_ui_->addParameter(param);
            }
        }
    }
    params_panel_.content().addChild(parameter_ui_.get());

    // --- Knobs Panel (top-right) ---
    addChild(&knobs_panel_);

    if (getParamsExtension()) {
        const auto findDst = [&](const char* name) { return mod_matrix ? mod_matrix->findDestination(name) : nullptr; };

        param1_knob_ =
            std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("param1"), findDst("param1"));
        knobs_panel_.content().addChild(param1_knob_.get());

        param2_knob_ =
            std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("param2"), findDst("param2"));
        knobs_panel_.content().addChild(param2_knob_.get());

        filter_mode_knob_ =
            std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("filter_mode"), findDst("filter_mode"));
        knobs_panel_.content().addChild(filter_mode_knob_.get());
    }

    // --- Buttons Panel (middle-right) ---
    addChild(&buttons_panel_);

    ui_button_ = std::make_unique<applause::Button>("BUTTON");
    ui_button_->onClick() += [] { LOG_INFO("UI Button clicked!"); };
    buttons_panel_.content().addChild(ui_button_.get());

    action_button_ = std::make_unique<applause::Button>("Action");
    action_button_->setRole(applause::Button::Role::Primary);
    action_button_->onClick() += [] { LOG_INFO("Action Button clicked!"); };
    buttons_panel_.content().addChild(action_button_.get());

    toggle_button_ = std::make_unique<applause::Button>("TOGGLE");
    toggle_button_->setCheckable(true);
    toggle_button_->onToggle() += [](bool on) { LOG_INFO("Toggle button state: {}", on ? "ON" : "OFF"); };
    buttons_panel_.content().addChild(toggle_button_.get());

    load_file_button_ = std::make_unique<applause::Button>(icon(kFolderIcon), "Load File");
    load_file_button_->onClick() += [this] { onLoadFileClicked(); };
    buttons_panel_.content().addChild(load_file_button_.get());

#ifdef __APPLE__
    popup_menu_button_ = std::make_unique<applause::Button>("Show Menu");
    popup_menu_button_->onClick() += [this] {
        applause::NativePopupMenu menu("Demo Menu");

        menu.addOption(1, "Option 1").select(true).withKeyboardShortcut(applause::NativePopupMenu::Modifier::Cmd, "1");

        menu.addOption(2, "Option 2").withKeyboardShortcut(applause::NativePopupMenu::Modifier::Cmd, "2");

        menu.addOption(3, "Disabled Option").enable(false);

        menu.addBreak();

        menu.addSubMenu("Submenu").addOption(10, "Sub Option A").addOption(11, "Sub Option B");

        menu.onSelection() += [](int id) { LOG_INFO("Popup menu selected: {}", id); };

        menu.onCancel() += []() { LOG_INFO("Popup menu cancelled"); };

        if (void* native_handle = getNativeHandle()) {
            auto pos = popup_menu_button_->positionInWindow();
            menu.show(native_handle, pos.x, pos.y + popup_menu_button_->height());
        }
    };
    buttons_panel_.content().addChild(popup_menu_button_.get());
#endif

    inactive_button_ = std::make_unique<applause::Button>("Inactive");
    inactive_button_->setEnabled(false);
    buttons_panel_.content().addChild(inactive_button_.get());

#ifndef NDEBUG
    inspector_button_ = std::make_unique<applause::Button>("Inspector");
    inspector_button_->setRole(applause::Button::Role::Primary);
    inspector_button_->onClick() += [this] {
        if (auto* w = inspector()) w->toggleShown();
    };
    buttons_panel_.content().addChild(inspector_button_.get());
    applause::setTooltip(*inspector_button_, "Open the UI Inspector (Cmd+I)");
#endif

    flat_button_ = std::make_unique<applause::Button>("Flat");
    flat_button_->setRole(applause::Button::Role::Flat);
    flat_button_->onClick() += [] { LOG_INFO("Flat button clicked!"); };
    buttons_panel_.content().addChild(flat_button_.get());

    star_button_ = std::make_unique<applause::Button>(icon(kStarIcon));
    star_button_->setRole(applause::Button::Role::Flat);
    star_button_->setCheckable(true);
    star_button_->onToggle() += [](bool on) { LOG_INFO("Star state: {}", on ? "ON" : "OFF"); };
    buttons_panel_.content().addChild(star_button_.get());
    applause::setTooltip(*star_button_, "A flat, checkable icon button");

    small_button_ = std::make_unique<applause::Button>("TINY");
    small_button_->onClick() += [] { LOG_INFO("Small button clicked!"); };
    buttons_panel_.content().addChild(small_button_.get());

    small_toggle_button_ = std::make_unique<applause::Button>("TINY");
    small_toggle_button_->setCheckable(true);
    small_toggle_button_->onToggle() += [](bool on) { LOG_INFO("Small toggle state: {}", on ? "ON" : "OFF"); };
    buttons_panel_.content().addChild(small_toggle_button_.get());

    checkbox_.setChecked(true);
    buttons_panel_.content().addChild(checkbox_);
    inactive_checkbox_.setEnabled(false);
    buttons_panel_.content().addChild(inactive_checkbox_);
    buttons_panel_.content().addChild(small_checkbox_);
    if (getParamsExtension()) {
        enabled_checkbox_ = std::make_unique<applause::ParamCheckbox>(getParamsExtension()->getInfo("demo_enabled"));
        buttons_panel_.content().addChild(enabled_checkbox_.get());
    }

    demo_radio_group_.addButton(first_radio_, 0);
    demo_radio_group_.addButton(second_radio_, 1);
    demo_radio_group_.addButton(inactive_radio_, 2);
    inactive_radio_.setEnabled(false);
    buttons_panel_.content().addChild(first_radio_);
    buttons_panel_.content().addChild(second_radio_);
    buttons_panel_.content().addChild(inactive_radio_);

    // --- Sliders Panel (right column) ---
    addChild(&sliders_panel_);

    bipolar_slider_.setBipolar(true);
    bipolar_slider_.setValue(0.0f);
    sliders_panel_.content().addChild(&bipolar_slider_);

    normal_slider_.setValue(0.5f);
    sliders_panel_.content().addChild(&normal_slider_);

    inactive_slider_.setValue(0.35f);
    inactive_slider_.setActive(false);
    sliders_panel_.content().addChild(&inactive_slider_);

    // --- Selection Grids Panel ---
    addChild(&selection_grids_panel_);

    if (getParamsExtension()) {
        filter_mode_grid_ = std::make_unique<applause::ParamSelectionGrid>(
            getParamsExtension()->getInfo("filter_mode"), 3, 2);
        selection_grids_panel_.content().addChild(filter_mode_grid_.get());

        auto& filter_mode = getParamsExtension()->getInfo("filter_mode");
        for (int index = 0; index < static_cast<int>(filter_radios_.size()); ++index) {
            auto radio = std::make_unique<applause::RadioButton>(filter_mode.valueToText(filter_mode.minValue + index));
            filter_radio_group_.addButton(*radio, index);
            selection_grids_panel_.content().addChild(radio.get());
            filter_radios_[index] = std::move(radio);
        }
        filter_radio_binding_.attach(filter_mode, filter_radio_group_);
    }

    waveform_grid_.emplaceCell<WaveformCell>(0, WaveformCell::Waveform::Sine);
    waveform_grid_.emplaceCell<WaveformCell>(1, WaveformCell::Waveform::Triangle);
    waveform_grid_.emplaceCell<WaveformCell>(2, WaveformCell::Waveform::SawUp);
    waveform_grid_.emplaceCell<WaveformCell>(3, WaveformCell::Waveform::SawDown);
    waveform_grid_.emplaceCell<WaveformCell>(4, WaveformCell::Waveform::Square);
    waveform_grid_.emplaceCell<WaveformCell>(5, WaveformCell::Waveform::Pulse);
    waveform_grid_.onSelectionChanged() +=
        [](int index) { LOG_INFO("Waveform grid selection: {}", index); };
    selection_grids_panel_.content().addChild(&waveform_grid_);

    // --- Tooltips ---
    if (ui_button_) applause::setTooltip(*ui_button_, "A regular button");
    if (action_button_) applause::setTooltip(*action_button_, "An action button with accent styling");
    if (toggle_button_) applause::setTooltip(*toggle_button_, "Click to toggle on/off");
    if (load_file_button_) applause::setTooltip(*load_file_button_, "Open a file dialog");
    applause::setTooltip(normal_slider_, "A unipolar slider");
    applause::setTooltip(bipolar_slider_, "A bipolar slider centered at zero");
    applause::setTooltip(checkbox_, "An independent two-state checkbox");
    if (enabled_checkbox_)
        applause::setTooltip(*enabled_checkbox_, "Connected to Demo Enabled in the parameter list");
    for (auto& radio : filter_radios_)
        if (radio) applause::setTooltip(*radio, "Shares Filter Mode with the knob, grid, and parameter list");

    // --- MSEG Panel ---
    addChild(&mseg_panel_);

    demo_curve_.points[0] = {0.0f, 0.0f};
    demo_curve_.points[1] = {0.2f, 0.8f};
    demo_curve_.points[2] = {0.5f, 1.0f};
    demo_curve_.points[3] = {0.8f, 0.3f};
    demo_curve_.points[4] = {1.0f, 0.0f};
    demo_curve_.num_points = 5;
    demo_curve_.curvature_power[0] = -2.0f;  // concave rise
    demo_curve_.curvature_power[3] = 3.0f;  // convex tail
    demo_curve_.loop = true;

    mseg_display_.setCurve(&demo_curve_);
    mseg_panel_.content().addChild(&mseg_plot_);
    mseg_plot_.addChild(&mseg_display_);

    // --- Trace and Plot Panel ---
    addChild(&plots_panel_);
    plots_panel_.content().addChild(&trace_demo_);
    plots_panel_.content().addChild(&plot_demo_);
    plot_demo_.addChild(&plot_trace_demo_);

    std::array<float, 256> samples;
    for (size_t i = 0; i < samples.size(); ++i) {
        const float x = static_cast<float>(i) / static_cast<float>(samples.size() - 1);
        samples[i] = 0.5f + 0.4f * std::sin(4.0f * std::numbers::pi_v<float> * x);
    }
    trace_demo_.setSamples(samples);

    for (size_t i = 0; i < samples.size(); ++i) {
        const float x = static_cast<float>(i) / static_cast<float>(samples.size() - 1);
        samples[i] = 1.0f / std::sqrt(1.0f + std::pow(x / 0.55f, 8.0f));
    }
    plot_trace_demo_.setSamples(samples);
    plot_demo_.setGridDivisions(6, 4);

    // --- Mod Matrix Panel ---
    addChild(&mod_matrix_panel_);

    if (mod_matrix) {
        mod_matrix_ui_ = std::make_unique<applause::ModMatrixComponent>(*mod_matrix);
        mod_matrix_panel_.content().addChild(mod_matrix_ui_.get());
    }

    // --- Theming Panel (far right) ---
    // A token set on a frame applies to that frame and everything inside it, and the nearest frame that sets a token
    // wins. Colors and values (sizes, roundings, ...) work the same way.
    addChild(&theming_panel_);
    for (auto* section : {&accent_panel_, &scoped_panel_, &items_panel_, &custom_panel_})
        theming_panel_.content().addChild(section);
    // Set on the content frame, so the section panels get compact titles but the Theming panel keeps its own.
    theming_panel_.content().set(applause::Panel::kTitleHeight, 30.0f);

    // The whole app: tokens set on the editor reach everything in it. These buttons change the shared accent at
    // runtime, which retints every control that follows it (knobs, sliders, toggles, glows, grids, and plots).
    // Each button's own text and glow are set on just that button. Light accents also get dark text on Primary
    // buttons, which are filled with the accent.
    const std::tuple<applause::Button*, applause::Color, applause::Color> accents[] = {
        {&purple_button_, applause::Color(0xff9966ff), applause::Color(0xffffffff)},
        {&teal_button_, applause::Color(0xff3cc8b4), applause::Color(0xff111115)},
        {&amber_button_, applause::Color(0xffffb03a), applause::Color(0xff111115)},
    };
    for (const auto& [button, accent, primary_text] : accents) {
        button->set(applause::Button::kText, accent);
        button->set(applause::Button::kTextHover, accent);
        button->set(applause::Button::kGlow, accent);
        button->onClick() += [this, accent, primary_text] {
            set(applause::theme::kAccent, accent);
            set(applause::Button::kPrimaryText, primary_text);
        };
        applause::setTooltip(*button, "Sets theme::kAccent and Button::kPrimaryText on the editor");
        accent_panel_.content().addChild(button);
    }

    // One subtree: tokens set on a panel win over the editor's inside it, so this panel keeps its own accent whichever
    // app accent is picked. Its background is set the same way.
    scoped_panel_.set(applause::theme::kAccent, applause::Color(0xffff5c8a));
    scoped_panel_.set(applause::Panel::kBackground, applause::Color(0xff231a20));
    scoped_toggle_.setCheckable(true);
    scoped_toggle_.setChecked(true);
    scoped_checkbox_.setChecked(true);
    scoped_slider_.setValue(0.6f);
    for (applause::Frame* item :
         std::initializer_list<applause::Frame*>{&scoped_toggle_, &scoped_checkbox_, &scoped_slider_, &scoped_badge_}) {
        scoped_panel_.content().addChild(item);
        applause::setTooltip(*item, "Follows theme::kAccent, set on the panel around it");
    }

    // Single items: a token set on one frame changes only that frame.
    pill_button_.set(applause::Button::kRounding, 17.0f);
    applause::setTooltip(pill_button_, "Button::kRounding set on this button");
    delete_button_.set(applause::Button::kText, applause::Color(0xffff6b6b));
    delete_button_.set(applause::Button::kTextHover, applause::Color(0xffff9b9b));
    delete_button_.set(applause::Button::kGlow, applause::Color(0xffff4d4d));
    applause::setTooltip(delete_button_, "Button text and glow set on this button");
    // A frame can also make a token follow another: this button's text tracks the app accent.
    accent_text_button_.set(applause::Button::kText, applause::theme::kAccent);
    accent_text_button_.set(applause::Button::kTextHover, applause::theme::kAccent);
    applause::setTooltip(accent_text_button_, "Button::kText follows theme::kAccent on this button");
    for (auto* button : {&pill_button_, &delete_button_, &accent_text_button_}) items_panel_.content().addChild(button);

    // A custom component declares its own tokens (see Badge.h) and themes like any built-in one. Setting the badge's
    // one main color here restyles its text, fill, and border together.
    saved_badge_.set(Badge::kColor, applause::Color(0xff4cd387));
    applause::setTooltip(accent_badge_, "A custom component; its tokens follow theme::kAccent");
    applause::setTooltip(saved_badge_, "Badge::kColor set on this badge");
    custom_panel_.content().addChild(&accent_badge_);
    custom_panel_.content().addChild(&saved_badge_);
}

void ExampleShowcaseEditor::resized() {
    static constexpr float kPadding = 12.0f;
    static constexpr float kGap = 10.0f;

    float col_h = height() - 2 * kPadding;

    // --- Column 1 (left): Parameters ---
    float col1_w = 340.0f;
    params_panel_.setBounds(kPadding, kPadding, col1_w, col_h);
    if (parameter_ui_)
        parameter_ui_->setBounds(0, 0, params_panel_.content().width(), params_panel_.content().height());

    // --- Column 2 (middle): Knobs, Sliders, Buttons ---
    float col2_x = kPadding + col1_w + kGap;
    float col2_w = 320.0f;

    static constexpr float kKnobPanelHeight = 120.0f;
    knobs_panel_.setBounds(col2_x, kPadding, col2_w, kKnobPanelHeight);

    if (param1_knob_ && param2_knob_ && filter_mode_knob_) {
        auto& kc = knobs_panel_.content();
        float knob_w = kc.width() / 3.0f;
        float knob_h = kc.height();
        param1_knob_->setBounds(0, 0, knob_w, knob_h);
        param2_knob_->setBounds(knob_w, 0, knob_w, knob_h);
        filter_mode_knob_->setBounds(knob_w * 2, 0, knob_w, knob_h);
    }

    static constexpr float kSlidersPanelHeight = 160.0f;
    float sliders_y = kPadding + kKnobPanelHeight + kGap;
    sliders_panel_.setBounds(col2_x, sliders_y, col2_w, kSlidersPanelHeight);

    {
        auto& sc = sliders_panel_.content();
        float slider_pad = 8.0f;
        float slider_h = 24.0f;
        float slider_gap = 6.0f;
        float sw = sc.width() - 2 * slider_pad;
        normal_slider_.setBounds(slider_pad, slider_pad, sw, slider_h);
        bipolar_slider_.setBounds(slider_pad, slider_pad + slider_h + slider_gap, sw, slider_h);
        inactive_slider_.setBounds(slider_pad, slider_pad + 2 * (slider_h + slider_gap), sw, slider_h);
    }

    float buttons_y = sliders_y + kSlidersPanelHeight + kGap;
    float buttons_h = col_h - kKnobPanelHeight - kSlidersPanelHeight - 2 * kGap;
    buttons_panel_.setBounds(col2_x, buttons_y, col2_w, buttons_h);

    {
        auto& bc = buttons_panel_.content();
        float btn_pad = 6.0f;
        float bw = (bc.width() - 2 * btn_pad - kGap) / 2.0f;
        float bh = 34.0f;
        float row_gap = 8.0f;
        int col = 0;
        int row = 0;

        auto placeButton = [&](applause::Frame* btn) {
            if (!btn) return;
            float x = btn_pad + col * (bw + kGap);
            float y = row * (bh + row_gap);
            btn->setBounds(x, y, bw, bh);
            col++;
            if (col >= 2) {
                col = 0;
                row++;
            }
        };

        placeButton(ui_button_.get());
        placeButton(action_button_.get());
        placeButton(toggle_button_.get());
        placeButton(load_file_button_.get());
#ifdef __APPLE__
        placeButton(popup_menu_button_.get());
#endif
        placeButton(inactive_button_.get());
#ifndef NDEBUG
        placeButton(inspector_button_.get());
#endif
        placeButton(flat_button_.get());
        placeButton(star_button_.get());

        if (col != 0) {
            col = 0;
            row++;
        }
        float small_bw = 70.0f;
        float small_bh = 24.0f;
        float small_y = row * (bh + row_gap);
        if (small_button_) small_button_->setBounds(btn_pad, small_y, small_bw, small_bh);
        if (small_toggle_button_)
            small_toggle_button_->setBounds(btn_pad + small_bw + kGap, small_y, small_bw, small_bh);

        const float checkbox_y = small_y + small_bh + row_gap;
        checkbox_.setBounds(btn_pad, checkbox_y, bw, 24.0f);
        if (enabled_checkbox_)
            enabled_checkbox_->setBounds(btn_pad + bw + kGap, checkbox_y, bw, 24.0f);
        inactive_checkbox_.setBounds(btn_pad, checkbox_y + 28.0f, bw, 24.0f);
        small_checkbox_.setBounds(btn_pad + bw + kGap, checkbox_y + 31.0f, bw, 18.0f);

        const float radio_y = checkbox_y + 60.0f;
        const float radio_w = (bc.width() - 2 * btn_pad) / 3.0f;
        first_radio_.setBounds(btn_pad, radio_y, radio_w, 24.0f);
        second_radio_.setBounds(btn_pad + radio_w, radio_y, radio_w, 24.0f);
        inactive_radio_.setBounds(btn_pad + 2 * radio_w, radio_y, radio_w, 24.0f);
    }

    // --- Column 3: MSEG, Selection Grids, Mod Matrix, Plots ---
    static constexpr float kThemingPanelWidth = 300.0f;
    float col3_x = col2_x + col2_w + kGap;
    float col3_w = width() - col3_x - kGap - kThemingPanelWidth - kPadding;

    float mseg_h = (col_h - kGap) * 0.5f;
    static constexpr float kSelectionGridsPanelWidth = 260.0f;
    float mseg_w = col3_w - kSelectionGridsPanelWidth - kGap;
    mseg_panel_.setBounds(col3_x, kPadding, mseg_w, mseg_h);
    mseg_plot_.setBounds(0, 0, mseg_panel_.content().width(), mseg_panel_.content().height());
    mseg_display_.setBounds(kGap, kGap,
                            std::max(0.0f, mseg_plot_.width() - 2.0f * kGap),
                            std::max(0.0f, mseg_plot_.height() - 2.0f * kGap));

    selection_grids_panel_.setBounds(col3_x + mseg_w + kGap, kPadding, kSelectionGridsPanelWidth, mseg_h);
    {
        auto& cc = selection_grids_panel_.content();
        static constexpr float kTextGridHeight = 72.0f;
        static constexpr float kGridGap = 8.0f;
        if (filter_mode_grid_)
            filter_mode_grid_->setBounds(0, 0, cc.width(), kTextGridHeight);

        constexpr float kRadioHeight = 24.0f;
        const float radio_y = kTextGridHeight + kGridGap;
        for (int index = 0; index < static_cast<int>(filter_radios_.size()); ++index) {
            if (filter_radios_[index])
                filter_radios_[index]->setBounds((index % 2) * cc.width() * 0.5f,
                                                radio_y + (index / 2) * kRadioHeight,
                                                cc.width() * 0.5f, kRadioHeight);
        }

        float waveform_y = radio_y + 3 * kRadioHeight + kGridGap;
        float waveform_h = cc.height() - waveform_y;
        float waveform_w = std::min(cc.width(), waveform_h * 2.0f / 3.0f);
        waveform_grid_.setBounds((cc.width() - waveform_w) * 0.5f, waveform_y, waveform_w, waveform_h);
    }

    float mod_y = kPadding + mseg_h + kGap;
    static constexpr float kPlotsPanelHeight = 140.0f;
    float mod_h = col_h - mseg_h - 2 * kGap - kPlotsPanelHeight;
    mod_matrix_panel_.setBounds(col3_x, mod_y, col3_w, mod_h);
    if (mod_matrix_ui_)
        mod_matrix_ui_->setBounds(0, 0, mod_matrix_panel_.content().width(), mod_matrix_panel_.content().height());

    plots_panel_.setBounds(col3_x, mod_y + mod_h + kGap, col3_w, kPlotsPanelHeight);
    auto& pc = plots_panel_.content();
    float plot_w = (pc.width() - kGap) * 0.5f;
    trace_demo_.setBounds(0, 0, plot_w, pc.height());
    plot_demo_.setBounds(plot_w + kGap, 0, plot_w, pc.height());
    plot_trace_demo_.setBounds(plot_demo_.localBounds());

    // --- Column 4 (far right): Theming ---
    theming_panel_.setBounds(col3_x + col3_w + kGap, kPadding, kThemingPanelWidth, col_h);
    {
        static constexpr float kRowHeight = 30.0f;
        static constexpr float kRowGap = 8.0f;
        const float section_w = theming_panel_.content().width();
        float section_y = 0.0f;
        // Sizes each section to its title and rows. Panel sizes are theme values too, read from the section itself.
        const auto placeSection = [&](applause::Panel& section, int rows) {
            const float h = applause::value(section, applause::Panel::kTitleHeight) + rows * (kRowHeight + kRowGap) +
                applause::value(section, applause::Panel::kContentMargin);
            section.setBounds(0, section_y, section_w, h);
            section_y += h + kGap;
        };
        const auto placeRow = [&](applause::Panel& section, int row, std::initializer_list<applause::Frame*> items) {
            const float item_w =
                (section.content().width() - (items.size() - 1) * kRowGap) / static_cast<float>(items.size());
            float x = 0.0f;
            for (auto* item : items) {
                item->setBounds(x, row * (kRowHeight + kRowGap), item_w, kRowHeight);
                x += item_w + kRowGap;
            }
        };

        placeSection(accent_panel_, 1);
        placeRow(accent_panel_, 0, {&purple_button_, &teal_button_, &amber_button_});

        placeSection(scoped_panel_, 3);
        placeRow(scoped_panel_, 0, {&scoped_toggle_, &scoped_checkbox_});
        placeRow(scoped_panel_, 1, {&scoped_slider_});
        placeRow(scoped_panel_, 2, {&scoped_badge_});

        placeSection(items_panel_, 2);
        placeRow(items_panel_, 0, {&pill_button_, &delete_button_});
        placeRow(items_panel_, 1, {&accent_text_button_});

        placeSection(custom_panel_, 1);
        placeRow(custom_panel_, 0, {&accent_badge_, &saved_badge_});
    }
}

void ExampleShowcaseEditor::onLoadFileClicked() {
    NFD::Guard nfdGuard;

    NFD::UniquePathU8 outPath;
    nfdu8filteritem_t filters[] = {{"Text Files", "txt,md,json,xml"}, {"All Files", "*"}};

    nfdresult_t result = NFD::OpenDialog(outPath, filters, 2);

    if (result == NFD_OKAY) {
        std::string path = outPath.get();
        LOG_INFO("User selected file: {}", path);

        std::ifstream file(path);
        if (!file.is_open()) {
            LOG_ERR("Failed to open file: {}", path);
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        LOG_DBG("File content ({} bytes):\n{}", content.size(), content);
    } else if (result == NFD_CANCEL) {
        LOG_INFO("User cancelled file dialog");
    } else {
        LOG_ERR("NFD error: {}", NFD::GetError());
    }
}
