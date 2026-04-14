#include "ExampleShowcaseEditor.h"
#ifdef __APPLE__
#include <applause/ui/NativePopupMenu.h>
#endif
#include <applause/ui/Tooltip.h>
#include <applause/util/DebugHelpers.h>
#include <fstream>
#include <nfd.hpp>
#include <sstream>
#include <visage_graphics/canvas.h>

using namespace visage::dimension;

ExampleShowcaseEditor::ExampleShowcaseEditor(applause::ParamsExtension* params, applause::ModMatrix* mod_matrix) :
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
        param1_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("param1"));
        knobs_panel_.content().addChild(param1_knob_.get());

        param2_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("param2"));
        knobs_panel_.content().addChild(param2_knob_.get());

        filter_mode_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("filter_mode"));
        knobs_panel_.content().addChild(filter_mode_knob_.get());
    }

    // --- Buttons Panel (middle-right) ---
    addChild(&buttons_panel_);

    ui_button_ = std::make_unique<applause::UiButton>("BUTTON");
    ui_button_->onToggle() += [](applause::Button* button, bool on) { LOG_INFO("UI Button clicked!"); };
    buttons_panel_.content().addChild(ui_button_.get());

    action_button_ = std::make_unique<applause::UiButton>("Action");
    action_button_->setActionButton();
    action_button_->onToggle() += [](applause::Button* button, bool on) { LOG_INFO("Action Button clicked!"); };
    buttons_panel_.content().addChild(action_button_.get());

    toggle_button_ = std::make_unique<applause::ToggleTextButton>("TOGGLE");
    toggle_button_->onToggle() +=
        [](applause::Button* button, bool on) { LOG_INFO("Toggle button state: {}", on ? "ON" : "OFF"); };
    buttons_panel_.content().addChild(toggle_button_.get());

    load_file_button_ = std::make_unique<applause::UiButton>("Load File");
    load_file_button_->onToggle() += [this](applause::Button* button, bool on) { onLoadFileClicked(); };
    buttons_panel_.content().addChild(load_file_button_.get());

#ifdef __APPLE__
    popup_menu_button_ = std::make_unique<applause::UiButton>("Show Menu");
    popup_menu_button_->onToggle() += [this](applause::Button* button, bool on) {
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

    inactive_button_ = std::make_unique<applause::UiButton>("Inactive");
    inactive_button_->setActive(false);
    buttons_panel_.content().addChild(inactive_button_.get());

    small_button_ = std::make_unique<applause::UiButton>("TINY");
    small_button_->onToggle() += [](applause::Button* button, bool on) { LOG_INFO("Small button clicked!"); };
    buttons_panel_.content().addChild(small_button_.get());

    small_toggle_button_ = std::make_unique<applause::ToggleTextButton>("TINY");
    small_toggle_button_->onToggle() +=
        [](applause::Button* button, bool on) { LOG_INFO("Small toggle state: {}", on ? "ON" : "OFF"); };
    buttons_panel_.content().addChild(small_toggle_button_.get());

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

    // --- Tooltips ---
    if (ui_button_) applause::setTooltip(*ui_button_, "A regular button");
    if (action_button_) applause::setTooltip(*action_button_, "An action button with accent styling");
    if (toggle_button_) applause::setTooltip(*toggle_button_, "Click to toggle on/off");
    if (load_file_button_) applause::setTooltip(*load_file_button_, "Open a file dialog");
    applause::setTooltip(normal_slider_, "A unipolar slider");
    applause::setTooltip(bipolar_slider_, "A bipolar slider centered at zero");

    // --- MSEG Panel ---
    addChild(&mseg_panel_);

    demo_curve_.points[0] = {0.0f, 0.0f};
    demo_curve_.points[1] = {0.2f, 0.8f};
    demo_curve_.points[2] = {0.5f, 1.0f};
    demo_curve_.points[3] = {0.8f, 0.3f};
    demo_curve_.points[4] = {1.0f, 0.0f};
    demo_curve_.num_points = 5;
    demo_curve_.curvature_power[0] = -2.0f; // concave rise
    demo_curve_.curvature_power[3] = 3.0f;  // convex tail
    demo_curve_.loop = true;

    mseg_display_.setCurve(&demo_curve_);
    mseg_panel_.content().addChild(&mseg_display_);

    // --- Mod Matrix Panel (bottom) ---
    addChild(&mod_matrix_panel_);

    if (mod_matrix) {
        mod_matrix_ui_ = std::make_unique<applause::ModMatrixComponent>(*mod_matrix);
        mod_matrix_panel_.content().addChild(mod_matrix_ui_.get());
    }
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

        auto placeButton = [&](visage::Frame* btn) {
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
    }

    // --- Column 3 (right): MSEG, Mod Matrix ---
    float col3_x = col2_x + col2_w + kGap;
    float col3_w = width() - col3_x - kPadding;

    float mseg_h = (col_h - kGap) * 0.45f;
    mseg_panel_.setBounds(col3_x, kPadding, col3_w, mseg_h);
    mseg_display_.setBounds(0, 0, mseg_panel_.content().width(), mseg_panel_.content().height());

    float mod_y = kPadding + mseg_h + kGap;
    float mod_h = col_h - mseg_h - kGap;
    mod_matrix_panel_.setBounds(col3_x, mod_y, col3_w, mod_h);
    if (mod_matrix_ui_)
        mod_matrix_ui_->setBounds(0, 0, mod_matrix_panel_.content().width(), mod_matrix_panel_.content().height());
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
