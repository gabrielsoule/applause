#include <catch2/catch_test_macros.hpp>

#include <applause/core/PluginBase.h>
#include <applause/ui/components/Button.h>
#include <applause/ui/components/ParamRadioGroup.h>
#include <applause/util/ParamMessageQueue.h>

#include <array>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

const clap_plugin_descriptor_t kDescriptor{};

struct TestPlugin : applause::PluginBase {
    applause::ParamsExtension params{1};

    TestPlugin() : PluginBase(&kDescriptor, nullptr) {
        registerExtension(params);
        params.registerParam({.string_id = "mode",
                              .min_value = 4.0f,
                              .max_value = 6.0f,
                              .default_value = 5.0f,
                              .is_stepped = true});
    }

    applause::ProcessStatus process(applause::ProcessContext&) noexcept override {
        return applause::ProcessStatus::Continue;
    }
};

void addButtons(applause::RadioGroup& group, std::array<applause::ToggleButton, 3>& buttons) {
    // Mapping follows IDs, independently of insertion order.
    group.addButton(buttons[2], 2);
    group.addButton(buttons[0], 0);
    group.addButton(buttons[1], 1);
}

void click(applause::ToggleButton& button) {
    button.setBounds(0.0f, 0.0f, 20.0f, 20.0f);
    applause::MouseEvent event;
    event.position = {1.0f, 1.0f};
    event.is_down = true;
    button.mouseDown(event);
    event.is_down = false;
    button.mouseUp(event);
}

void requireEmpty(applause::ParamMessageQueue& queue) {
    applause::ParamMessageQueue::Message message{};
    REQUIRE_FALSE(queue.toAudio().try_dequeue(message));
}

void requireGesture(applause::ParamMessageQueue& queue, clap_id id, float value) {
    applause::ParamMessageQueue::Message message{};
    REQUIRE(queue.toAudio().try_dequeue(message));
    REQUIRE(message.type == applause::ParamMessageQueue::BEGIN_GESTURE);
    REQUIRE(message.paramId == id);
    REQUIRE(queue.toAudio().try_dequeue(message));
    REQUIRE(message.type == applause::ParamMessageQueue::PARAM_VALUE);
    REQUIRE(message.paramId == id);
    REQUIRE(message.value == value);
    REQUIRE(queue.toAudio().try_dequeue(message));
    REQUIRE(message.type == applause::ParamMessageQueue::END_GESTURE);
    REQUIRE(message.paramId == id);
    requireEmpty(queue);
}

}  // namespace

TEST_CASE("ParamRadioGroup validates parameter range and member IDs", "[ui][radio-group][params]") {
    TestPlugin plugin;
    auto& parameter = plugin.params.getInfo("mode");
    std::array<applause::ToggleButton, 3> buttons;
    applause::RadioGroup group;

    SECTION("continuous parameter") {
        addButtons(group, buttons);
        parameter.stepped = false;
        REQUIRE_THROWS_AS(applause::ParamRadioGroup(parameter, group), std::invalid_argument);
    }
    SECTION("fractional bounds") {
        addButtons(group, buttons);
        parameter.minValue = 3.5f;
        REQUIRE_THROWS_AS(applause::ParamRadioGroup(parameter, group), std::invalid_argument);
    }
    SECTION("IDs are sparse") {
        group.addButton(buttons[0], 0);
        group.addButton(buttons[1], 1);
        group.addButton(buttons[2], 3);
        REQUIRE_THROWS_AS(applause::ParamRadioGroup(parameter, group), std::invalid_argument);
    }
}

TEST_CASE("ParamRadioGroup attaches later without notifications and synchronizes subsequent changes",
          "[ui][radio-group][params]") {
    TestPlugin plugin;
    auto& parameter = plugin.params.getInfo("mode");
    applause::ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    std::array<applause::ToggleButton, 3> buttons;
    applause::RadioGroup group;
    applause::ParamRadioGroup binding;
    addButtons(group, buttons);
    int initialization_changes = 0;
    group.onSelectionChanged() += [&](int) { ++initialization_changes; };
    binding.attach(parameter, group);
    REQUIRE(group.selectedId() == 1);
    REQUIRE(buttons[1].toggled());
    REQUIRE(initialization_changes == 0);
    requireEmpty(queue);
    group.onSelectionChanged().clear();

    std::vector<int> changes;
    group.onSelectionChanged() += [&](int id) {
        REQUIRE(parameter.getValue() == 4.0f + id);
        REQUIRE(group.selectedId() == id);
        for (int i = 0; i < 3; ++i) REQUIRE(buttons[i].toggled() == (i == id));
        requireGesture(queue, parameter.clapId, 4.0f + id);
        changes.push_back(id);
    };

    buttons[2].onToggle().clear();
    click(buttons[2]);
    REQUIRE(changes == std::vector<int>{2});
    click(buttons[2]);
    REQUIRE(changes == std::vector<int>{2});
    requireEmpty(queue);

    parameter.setValueSilently(4.0f);
    parameter.on_value_changed(4.0f);
    REQUIRE(group.selectedId() == 0);
    REQUIRE(changes == std::vector<int>{2});
    requireEmpty(queue);

    group.onSelectionChanged().clear();
    click(buttons[1]);
    REQUIRE(parameter.getValue() == 5.0f);
    requireGesture(queue, parameter.clapId, 5.0f);
}

TEST_CASE("ParamRadioGroup can retry attachment after validation fails", "[ui][radio-group][params]") {
    TestPlugin plugin;
    auto& parameter = plugin.params.getInfo("mode");
    applause::ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    std::array<applause::ToggleButton, 3> buttons;
    applause::RadioGroup group;
    group.addButton(buttons[0], 0);
    group.addButton(buttons[1], 1);
    applause::ParamRadioGroup binding;

    REQUIRE_THROWS_AS(binding.attach(parameter, group), std::invalid_argument);
    parameter.on_value_changed(5.0f);
    REQUIRE(group.selectedId() == 0);
    requireEmpty(queue);
    group.addButton(buttons[2], 2);
    binding.attach(parameter, group);
    REQUIRE(group.selectedId() == 1);
    requireEmpty(queue);
    click(buttons[0]);
    REQUIRE(parameter.getValue() == 4.0f);
    requireGesture(queue, parameter.clapId, 4.0f);
}

TEST_CASE("ParamRadioGroup fixes member IDs while allowing atomic replacement", "[ui][radio-group][params]") {
    TestPlugin plugin;
    auto& parameter = plugin.params.getInfo("mode");
    applause::ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    std::array<applause::ToggleButton, 3> buttons;
    applause::ToggleButton replacement;
    applause::RadioGroup group;
    addButtons(group, buttons);

    {
        applause::ParamRadioGroup binding(parameter, group);
        REQUIRE_THROWS_AS(group.addButton(replacement, 3), std::logic_error);
        REQUIRE_THROWS_AS(group.removeButton(buttons[0]), std::logic_error);
        REQUIRE_THROWS_AS(binding.attach(parameter, group), std::logic_error);
        REQUIRE_THROWS_AS(applause::ParamRadioGroup(parameter, group), std::logic_error);

        group.replaceButton(1, replacement);
        REQUIRE(group.selectedId() == 1);
        REQUIRE(replacement.toggled());
        requireEmpty(queue);
        parameter.setValueSilently(6.0f);
        parameter.on_value_changed(6.0f);
        REQUIRE(group.selectedId() == 2);
        REQUIRE_FALSE(replacement.toggled());
        requireEmpty(queue);
        click(replacement);
        REQUIRE(parameter.getValue() == 5.0f);
        requireGesture(queue, parameter.clapId, 5.0f);
    }

    group.removeButton(replacement);
    group.setSelectedIdAndNotify(0);
    REQUIRE(parameter.getValue() == 5.0f);
    requireEmpty(queue);
}

TEST_CASE("ParamRadioGroup detaches safely when a member or group is destroyed", "[ui][radio-group][params]") {
    TestPlugin plugin;
    auto& parameter = plugin.params.getInfo("mode");
    applause::ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    const auto initial_connections = parameter.on_value_changed.get_slot_count();

    SECTION("member is destroyed before its binding") {
        applause::ToggleButton first;
        auto selected = std::make_unique<applause::ToggleButton>();
        applause::ToggleButton last;
        applause::RadioGroup group;
        group.addButton(first, 0);
        group.addButton(*selected, 1);
        group.addButton(last, 2);
        applause::ParamRadioGroup binding(parameter, group);
        REQUIRE(parameter.on_value_changed.get_slot_count() == initial_connections + 1);
        int changes = 0;
        group.onSelectionChanged() += [&](int) { ++changes; };

        selected.reset();
        REQUIRE(parameter.on_value_changed.get_slot_count() == initial_connections);
        REQUIRE(changes == 0);
        requireEmpty(queue);
        parameter.on_value_changed(6.0f);
        REQUIRE(group.selectedId() == 0);
        group.setSelectedIdAndNotify(2);
        REQUIRE(parameter.getValue() == 5.0f);
        requireEmpty(queue);
    }

    SECTION("group is destroyed before its binding") {
        std::array<applause::ToggleButton, 3> buttons;
        auto group = std::make_unique<applause::RadioGroup>();
        addButtons(*group, buttons);
        applause::ParamRadioGroup binding(parameter, *group);
        REQUIRE(parameter.on_value_changed.get_slot_count() == initial_connections + 1);
        group.reset();
        REQUIRE(parameter.on_value_changed.get_slot_count() == initial_connections);
        parameter.on_value_changed(4.0f);
        requireEmpty(queue);
    }
}

TEST_CASE("ParamRadioGroup synchronizes controls bound to the same parameter", "[ui][radio-group][params]") {
    TestPlugin plugin;
    auto& parameter = plugin.params.getInfo("mode");
    applause::ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    std::array<applause::ToggleButton, 3> first_buttons;
    std::array<applause::ToggleButton, 3> second_buttons;
    applause::RadioGroup first_group;
    applause::RadioGroup second_group;
    addButtons(first_group, first_buttons);
    addButtons(second_group, second_buttons);
    applause::ParamRadioGroup first_binding(parameter, first_group);
    applause::ParamRadioGroup second_binding(parameter, second_group);
    int second_changes = 0;
    second_group.onSelectionChanged() += [&](int) { ++second_changes; };

    click(first_buttons[2]);
    REQUIRE(first_group.selectedId() == 2);
    REQUIRE(second_group.selectedId() == 2);
    REQUIRE(second_changes == 0);
    requireGesture(queue, parameter.clapId, 6.0f);
}
