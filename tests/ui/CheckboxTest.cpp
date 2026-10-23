#include <catch2/catch_test_macros.hpp>

#include <applause/core/PluginBase.h>
#include <applause/extensions/ParamsExtension.h>
#include <applause/ui/components/Checkbox.h>
#include <applause/ui/components/ParamCheckbox.h>
#include <applause/ui/components/RadioButton.h>
#include <applause/ui/components/RadioGroup.h>
#include <applause/util/ParamMessageQueue.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const clap_plugin_descriptor_t kDescriptor{};

struct TestPlugin : applause::PluginBase {
    applause::ParamsExtension params{8};

    TestPlugin() : PluginBase(&kDescriptor, nullptr) { registerExtension(params); }
    applause::ProcessStatus process(applause::ProcessContext&) noexcept override {
        return applause::ProcessStatus::Continue;
    }
};

applause::ParamConfig binaryParam(float min = 0.0f, float max = 1.0f, float value = 0.0f) {
    return applause::ParamConfig{
        .string_id = "enabled",
        .name = "Effect enabled",
        .min_value = min,
        .max_value = max,
        .default_value = value,
        .is_stepped = true,
    };
}

void click(applause::Button& button, applause::Point point = {1.0f, 1.0f}, bool release_inside = true) {
    applause::MouseEvent down;
    down.position = point;
    down.is_down = true;
    button.mouseDown(down);

    applause::MouseEvent up;
    up.position = release_inside ? point : applause::Point{-1.0f, -1.0f};
    button.mouseUp(up);
}

void requireGesture(applause::ParamMessageQueue& queue, const applause::ParamInfo& parameter, float value) {
    applause::ParamMessageQueue::Message message{};
    REQUIRE(queue.toAudio().try_dequeue(message));
    REQUIRE(message.type == applause::ParamMessageQueue::BEGIN_GESTURE);
    REQUIRE(message.paramId == parameter.clapId);
    REQUIRE(queue.toAudio().try_dequeue(message));
    REQUIRE(message.type == applause::ParamMessageQueue::PARAM_VALUE);
    REQUIRE(message.paramId == parameter.clapId);
    REQUIRE(message.value == value);
    REQUIRE(queue.toAudio().try_dequeue(message));
    REQUIRE(message.type == applause::ParamMessageQueue::END_GESTURE);
    REQUIRE(message.paramId == parameter.clapId);
    REQUIRE_FALSE(queue.toAudio().try_dequeue(message));
}

}  // namespace

TEST_CASE("Checkboxes toggle independently across the whole label", "[ui][checkbox]") {
    applause::Checkbox first("First");
    applause::Checkbox second("Second");
    first.setBounds(0.0f, 0.0f, 120.0f, 24.0f);
    second.setBounds(0.0f, 0.0f, 120.0f, 24.0f);
    std::vector<bool> changes;
    first.onToggle() += [&](bool on) { changes.push_back(on); };

    REQUIRE(first.frameAtPoint({90.0f, 12.0f}) == &first);
    click(first, {90.0f, 12.0f});
    REQUIRE(first.isChecked());
    REQUIRE_FALSE(second.isChecked());
    click(second);
    REQUIRE(first.isChecked());
    REQUIRE(second.isChecked());
    click(first);
    REQUIRE_FALSE(first.isChecked());
    REQUIRE(second.isChecked());
    REQUIRE(changes == std::vector<bool>{true, false});
}

TEST_CASE("Labeled toggles distinguish silent state changes and notifications", "[ui][checkbox][radio-button]") {
    applause::RadioButton button("Mode");
    std::vector<bool> changes;
    button.onToggle() += [&](bool on) { changes.push_back(on); };

    button.setChecked(true);
    REQUIRE(button.isChecked());
    REQUIRE(changes.empty());
    button.setCheckedAndNotify(false);
    button.setCheckedAndNotify(false);
    REQUIRE_FALSE(button.isChecked());
    REQUIRE(changes == std::vector<bool>{false});
}

TEST_CASE("Checkbox activation respects inactive state and release position", "[ui][checkbox]") {
    applause::Checkbox button("Enabled");
    button.setBounds(0.0f, 0.0f, 120.0f, 24.0f);
    int changes = 0;
    button.onToggle() += [&](bool) { ++changes; };

    button.setEnabled(false);
    click(button);
    REQUIRE_FALSE(button.isChecked());
    REQUIRE(changes == 0);
    button.setEnabled(true);
    click(button, {90.0f, 12.0f}, false);
    REQUIRE_FALSE(button.isChecked());
    REQUIRE(changes == 0);

    button.setTriggeredOnMouseDown(true);
    click(button, {90.0f, 12.0f}, false);
    REQUIRE(button.isChecked());
    REQUIRE(changes == 1);
}

TEST_CASE("ParamCheckbox maps both endpoints and derives an overridable label", "[ui][checkbox][params]") {
    for (const float min : {-3.0f, 0.0f, 4.0f}) {
        CAPTURE(min);
        TestPlugin plugin;
        plugin.params.registerParam(binaryParam(min, min + 1.0f, min + 1.0f));
        auto& parameter = plugin.params.getInfo("enabled");
        applause::ParamCheckbox checkbox(parameter);
        REQUIRE(checkbox.isChecked());

        checkbox.setCheckedAndNotify(false);
        REQUIRE(parameter.getValue() == min);
        checkbox.setCheckedAndNotify(true);
        REQUIRE(parameter.getValue() == min + 1.0f);
    }

    TestPlugin plugin;
    plugin.params.registerParam(binaryParam());
    auto& parameter = plugin.params.getInfo("enabled");
    applause::ParamCheckbox full_label(parameter);
    REQUIRE(full_label.text().toUtf8() == "Effect enabled");
    parameter.shortName = "Enable";
    applause::ParamCheckbox short_label(parameter);
    REQUIRE(short_label.text().toUtf8() == "Enable");
    applause::ParamCheckbox explicit_label(parameter, "Override");
    REQUIRE(explicit_label.text().toUtf8() == "Override");
}

TEST_CASE("ParamCheckbox sends one complete gesture and synchronizes controls before callbacks",
          "[ui][checkbox][params]") {
    TestPlugin plugin;
    plugin.params.registerParam(binaryParam(-1.0f, 0.0f, -1.0f));
    auto& parameter = plugin.params.getInfo("enabled");
    applause::ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    applause::ParamCheckbox checkbox(parameter);
    applause::ParamCheckbox mirror(parameter);
    checkbox.setBounds(0.0f, 0.0f, 120.0f, 24.0f);
    std::vector<bool> changes;
    int mirror_changes = 0;
    checkbox.onToggle() = [&](bool on) {
        REQUIRE(parameter.getValue() == (on ? 0.0f : -1.0f));
        REQUIRE(mirror.isChecked() == on);
        changes.push_back(on);
    };
    mirror.onToggle() += [&](bool) { ++mirror_changes; };

    click(checkbox, {90.0f, 12.0f});
    REQUIRE(checkbox.isChecked());
    REQUIRE(changes == std::vector<bool>{true});
    REQUIRE(mirror_changes == 0);
    requireGesture(queue, parameter, 0.0f);

    // Already on, so nothing changes and nothing is sent.
    checkbox.setCheckedAndNotify(true);
    applause::ParamMessageQueue::Message message{};
    REQUIRE_FALSE(queue.toAudio().try_dequeue(message));
    REQUIRE(changes == std::vector<bool>{true});

    parameter.setValueSilently(-1.0f);
    parameter.on_value_changed(-1.0f);
    REQUIRE_FALSE(checkbox.isChecked());
    REQUIRE_FALSE(mirror.isChecked());
    REQUIRE(changes == std::vector<bool>{true});
    REQUIRE_FALSE(queue.toAudio().try_dequeue(message));

    checkbox.onToggle().clear();
    click(checkbox);
    REQUIRE(parameter.getValue() == 0.0f);
    REQUIRE(mirror.isChecked());
    REQUIRE(changes == std::vector<bool>{true});
    REQUIRE(mirror_changes == 0);
    requireGesture(queue, parameter, 0.0f);

    checkbox.setChecked(false);
    REQUIRE(parameter.getValue() == 0.0f);
    REQUIRE_FALSE(queue.toAudio().try_dequeue(message));
}

TEST_CASE("ParamCheckbox rejects nonbinary parameter ranges", "[ui][checkbox][params]") {
    TestPlugin plugin;
    plugin.params.registerParam(binaryParam());
    auto& parameter = plugin.params.getInfo("enabled");

    SECTION("continuous") { parameter.stepped = false; }
    SECTION("fractional bounds") {
        parameter.minValue = 0.5f;
        parameter.maxValue = 1.5f;
    }
    SECTION("more than two values") { parameter.maxValue = 2.0f; }
    SECTION("one value") { parameter.maxValue = 0.0f; }
    SECTION("infinite maximum") { parameter.maxValue = std::numeric_limits<float>::infinity(); }

    REQUIRE_THROWS_AS(applause::ParamCheckbox(parameter), std::invalid_argument);
}

TEST_CASE("ParamCheckbox disconnects from its parameter when destroyed", "[ui][checkbox][params]") {
    TestPlugin plugin;
    plugin.params.registerParam(binaryParam());
    auto& parameter = plugin.params.getInfo("enabled");
    const auto initial_connections = parameter.on_value_changed.get_slot_count();

    {
        applause::ParamCheckbox checkbox(parameter);
        REQUIRE(parameter.on_value_changed.get_slot_count() == initial_connections + 1);
    }
    REQUIRE(parameter.on_value_changed.get_slot_count() == initial_connections);
    parameter.setValueNotifyingHost(1.0f);
}

TEST_CASE("ParamCheckbox cannot join a radio group", "[ui][checkbox][params][radio-group]") {
    TestPlugin plugin;
    plugin.params.registerParam(binaryParam());
    applause::ParamCheckbox checkbox(plugin.params.getInfo("enabled"));
    applause::RadioGroup group;
    REQUIRE_THROWS_AS(group.addButton(checkbox, 0), std::invalid_argument);
}
