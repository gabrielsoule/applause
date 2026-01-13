#include "ExampleShowcaseEditor.h"
#ifdef __APPLE__
#include "applause/ui/NativePopupMenu.h"
#endif
#include "applause/util/DebugHelpers.h"
#include <visage_graphics/canvas.h>
#include <nfd.hpp>
#include <fstream>
#include <sstream>

using namespace visage::dimension;

ExampleShowcaseEditor::ExampleShowcaseEditor(applause::ParamsExtension* params)
    : applause::ApplauseEditor(params) {
    ApplauseEditor::setFixedAspectRatio(true);

    // Create parameter UI
    parameter_ui_ = std::make_unique<applause::GenericParameterUI>();

    // Add all non-internal parameters to the UI
    if (getParamsExtension()) {
        for (auto& param : getParamsExtension()->getAllParameters()) {
            if (!param.internal) {
                parameter_ui_->addParameter(param);
            }
        }
    }

    addChild(parameter_ui_.get());

    // Create individual knobs for each parameter
    if (getParamsExtension()) {
        param1_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("param1"));
        addChild(param1_knob_.get());

        param2_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("param2"));
        addChild(param2_knob_.get());

        filter_mode_knob_ = std::make_unique<applause::ParamKnob>(getParamsExtension()->getInfo("filter_mode"));
        addChild(filter_mode_knob_.get());
    }

    // Create test buttons
    ui_button_ = std::make_unique<applause::UiButton>("UI Button");
    ui_button_->onToggle() += [](applause::Button* button, bool on) {
        LOG_INFO("UI Button clicked!");
    };
    addChild(ui_button_.get());

    toggle_button_ = std::make_unique<applause::ToggleTextButton>("Toggle");
    toggle_button_->onToggle() += [](applause::Button* button, bool on) {
        LOG_INFO("Toggle button state: {}", on ? "ON" : "OFF");
    };
    addChild(toggle_button_.get());

    load_file_button_ = std::make_unique<applause::UiButton>("Load File");
    load_file_button_->onToggle() += [this](applause::Button* button, bool on) {
        onLoadFileClicked();
    };
    addChild(load_file_button_.get());

#ifdef __APPLE__
    popup_menu_button_ = std::make_unique<applause::UiButton>("Show Menu");
    popup_menu_button_->onToggle() += [this](applause::Button* button, bool on) {
        applause::NativePopupMenu menu("Demo Menu");

        menu.addOption(1, "Option 1")
            .select(true)
            .withKeyboardShortcut(applause::NativePopupMenu::Modifier::Cmd, "1");

        menu.addOption(2, "Option 2")
            .withKeyboardShortcut(applause::NativePopupMenu::Modifier::Cmd, "2");

        menu.addOption(3, "Disabled Option")
            .enable(false);

        menu.addBreak();

        menu.addSubMenu("Submenu")
            .addOption(10, "Sub Option A")
            .addOption(11, "Sub Option B");

        menu.onSelection() += [](int id) {
            LOG_INFO("Popup menu selected: {}", id);
        };

        menu.onCancel() += []() {
            LOG_INFO("Popup menu cancelled");
        };

        if (void* native_handle = getNativeHandle()) {
            menu.show(native_handle,
                      popup_menu_button_->x(),
                      popup_menu_button_->y() + popup_menu_button_->height());
        }
    };
    addChild(popup_menu_button_.get());
#endif
}

void ExampleShowcaseEditor::resized() {
    // Layout components
    parameter_ui_->setBounds(20, 20, 400, 400);

    // Position knobs in a vertical column to the right of GenericParameterUI
    const int knob_x = 450;
    const int knob_size = 50;
    const int knob_spacing = 100;
    const int knob_start_y = 40;

    if (param1_knob_)
        param1_knob_->setBounds(knob_x, knob_start_y, knob_size, knob_size + 20);

    if (param2_knob_)
        param2_knob_->setBounds(knob_x, knob_start_y + knob_spacing, knob_size, knob_size + 20);

    if (filter_mode_knob_)
        filter_mode_knob_->setBounds(knob_x, knob_start_y + knob_spacing * 2, knob_size, knob_size + 20);

    // Position buttons in a column to the right of the knobs
    const int button_x = knob_x + knob_size + 30;
    const int button_width = 120;
    const int button_height = 40;
    const int button_spacing = 50;

    if (ui_button_)
        ui_button_->setBounds(button_x, knob_start_y + 20, button_width, button_height);

    if (toggle_button_)
        toggle_button_->setBounds(button_x, knob_start_y + 20 + button_spacing, button_width, button_height);

    if (load_file_button_)
        load_file_button_->setBounds(button_x, knob_start_y + 20 + button_spacing * 2, button_width, button_height);

#ifdef __APPLE__
    if (popup_menu_button_)
        popup_menu_button_->setBounds(button_x, knob_start_y + 20 + button_spacing * 3, button_width, button_height);
#endif
}

void ExampleShowcaseEditor::onLoadFileClicked() {
    NFD::Guard nfdGuard;

    NFD::UniquePathU8 outPath;
    nfdu8filteritem_t filters[] = {
        {"Text Files", "txt,md,json,xml"},
        {"All Files", "*"}
    };

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
