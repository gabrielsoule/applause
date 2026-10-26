#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <applause/core/PluginBase.h>
#include <applause/extensions/ParamsExtension.h>
#include <applause/util/ParamMessageQueue.h>
#include <applause/util/ValueScaling.h>

using namespace applause;
using Catch::Approx;

namespace {
const clap_plugin_descriptor_t kDesc{};

struct TestPlugin : PluginBase {
    ParamsExtension params{8};
    explicit TestPlugin(const clap_host_t* host = nullptr) : PluginBase(&kDesc, host) { registerExtension(params); }
    ProcessStatus process(ProcessContext&) noexcept override { return ProcessStatus::Continue; }
};

// Room for 4 static parameters and 3 dynamic ones
struct DynamicPlugin : PluginBase {
    ParamsExtension params{4, 3};
    explicit DynamicPlugin(const clap_host_t* host = nullptr) : PluginBase(&kDesc, host) {
        registerExtension(params);
    }
    ProcessStatus process(ProcessContext&) noexcept override { return ProcessStatus::Continue; }
};

// Room for 4 static parameters, plus dynamic parameters and DAW automation slots
struct SlotPlugin : PluginBase {
    ParamsExtension params;
    explicit SlotPlugin(const clap_host_t* host = nullptr, uint32_t slots = 2, uint32_t dynamic = 3) :
        PluginBase(&kDesc, host), params(4, dynamic, slots) {
        registerExtension(params);
    }
    ProcessStatus process(ProcessContext&) noexcept override { return ProcessStatus::Continue; }
};

struct EventList {
    std::vector<clap_event_param_value_t> events;
    clap_input_events_t in{
        .ctx = this,
        .size = [](const clap_input_events_t* list) -> uint32_t {
            return static_cast<uint32_t>(static_cast<EventList*>(list->ctx)->events.size());
        },
        .get = [](const clap_input_events_t* list, uint32_t index) -> const clap_event_header_t* {
            return &static_cast<EventList*>(list->ctx)->events[index].header;
        },
    };
};

struct OutList {
    std::vector<clap_event_param_value_t> values;
    std::vector<clap_event_param_gesture_t> gestures;
    std::vector<uint16_t> types;
    clap_output_events_t out{
        .ctx = this,
        .try_push = [](const clap_output_events_t* list, const clap_event_header_t* event) -> bool {
            auto* self = static_cast<OutList*>(list->ctx);
            self->types.push_back(event->type);
            if (event->type == CLAP_EVENT_PARAM_VALUE)
                self->values.push_back(*reinterpret_cast<const clap_event_param_value_t*>(event));
            else if (event->type == CLAP_EVENT_PARAM_GESTURE_BEGIN || event->type == CLAP_EVENT_PARAM_GESTURE_END)
                self->gestures.push_back(*reinterpret_cast<const clap_event_param_gesture_t*>(event));
            return true;
        },
    };
};

struct FakeHost {
    int rescan_count = 0;
    clap_param_rescan_flags last_rescan_flags = 0;
    int flush_count = 0;
    int callback_count = 0;
    clap_host_params_t params{};
    clap_host_t host{};

    FakeHost() {
        params.rescan = [](const clap_host_t* h, clap_param_rescan_flags flags) {
            auto* self = static_cast<FakeHost*>(h->host_data);
            self->rescan_count++;
            self->last_rescan_flags = flags;
        };
        params.request_flush = [](const clap_host_t* h) { static_cast<FakeHost*>(h->host_data)->flush_count++; };
        host.host_data = this;
        host.request_callback = [](const clap_host_t* h) { static_cast<FakeHost*>(h->host_data)->callback_count++; };
        host.get_extension = [](const clap_host_t* h, const char* id) -> const void* {
            auto* self = static_cast<FakeHost*>(h->host_data);
            return std::strcmp(id, CLAP_EXT_PARAMS) == 0 ? static_cast<const void*>(&self->params) : nullptr;
        };
    }
};

clap_event_param_value_t makeEvent(clap_id param_id, void* cookie, double value) {
    clap_event_param_value_t event{};
    event.header.size = sizeof(event);
    event.header.type = CLAP_EVENT_PARAM_VALUE;
    event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    event.param_id = param_id;
    event.cookie = cookie;
    event.note_id = -1;
    event.port_index = -1;
    event.channel = -1;
    event.key = -1;
    event.value = value;
    return event;
}

ParamConfig rangedConfig(std::string string_id, float min, float max, float def, bool stepped = false) {
    ParamConfig config;
    config.string_id = std::move(string_id);
    config.min_value = min;
    config.max_value = max;
    config.default_value = def;
    config.is_stepped = stepped;
    return config;
}

ParamConfig makeConfig(std::string string_id, float default_value) {
    return rangedConfig(std::move(string_id), 0.0f, 1.0f, default_value);
}

ParamConfig choiceConfig(std::string string_id, float min, float def, std::vector<std::string> choices) {
    ParamConfig config;
    config.string_id = std::move(string_id);
    config.min_value = min;
    config.default_value = def;
    config.choices = std::move(choices);
    return config;
}

template <typename Plugin>
const clap_plugin_params_t* clapParams(Plugin& plugin) {
    return static_cast<const clap_plugin_params_t*>(plugin.params.getClapExtensionStruct());
}
}  // namespace

TEST_CASE("ParamsExtension applies host param_value via cookie", "[params][cookie]") {
    TestPlugin plugin;
    plugin.params.registerParam(makeConfig("a", 0.5f));
    plugin.params.registerParam(makeConfig("b", 0.5f));

    const clap_id id = plugin.params.getInfo("a").clapId;

    auto* clap_params = clapParams(plugin);
    clap_param_info_t info{};
    REQUIRE(clap_params->get_info(plugin.clapPlugin(), 0, &info));
    REQUIRE(info.id == id);
    REQUIRE(info.cookie != nullptr);

    SECTION("valid cookie fast path") {
        EventList list;
        list.events.push_back(makeEvent(id, info.cookie, 0.25));
        plugin.params.processEvents(&list.in, nullptr);
        REQUIRE(plugin.params.getInfo("a").getValue() == 0.25f);
    }

    SECTION("null cookie falls back to id lookup") {
        EventList list;
        list.events.push_back(makeEvent(id, nullptr, 0.75));
        plugin.params.processEvents(&list.in, nullptr);
        REQUIRE(plugin.params.getInfo("a").getValue() == 0.75f);
    }
}

TEST_CASE("ParamsExtension cookie routes to the correct parameter", "[params][cookie]") {
    TestPlugin plugin;
    plugin.params.registerParam(makeConfig("a", 0.1f));
    plugin.params.registerParam(makeConfig("b", 0.2f));

    auto* clap_params = clapParams(plugin);
    clap_param_info_t info_b{};
    REQUIRE(clap_params->get_info(plugin.clapPlugin(), 1, &info_b));
    REQUIRE(info_b.cookie != nullptr);

    EventList list;
    list.events.push_back(makeEvent(info_b.id, info_b.cookie, 0.9));
    plugin.params.processEvents(&list.in, nullptr);

    REQUIRE(plugin.params.getInfo("b").getValue() == 0.9f);
    REQUIRE(plugin.params.getInfo("a").getValue() == 0.1f);
}

TEST_CASE("ParamsExtension registration metadata via get_info", "[params][info]") {
    TestPlugin plugin;
    auto gain = rangedConfig("gain", -12.0f, 12.0f, 0.0f);
    gain.module = "Filter/Env";
    gain.unit = "dB";
    plugin.params.registerParam(gain);
    auto mode = rangedConfig("mode", 0.0f, 4.0f, 0.0f, true);
    mode.is_hidden = true;
    plugin.params.registerParam(mode);

    auto* clap_params = clapParams(plugin);
    clap_param_info_t info{};

    SECTION("name falls back to string_id") {
        REQUIRE(clap_params->get_info(plugin.clapPlugin(), 0, &info));
        REQUIRE(std::string(info.name) == "gain");
    }

    SECTION("module and range copied") {
        REQUIRE(clap_params->get_info(plugin.clapPlugin(), 0, &info));
        REQUIRE(std::string(info.module) == "Filter/Env");
        REQUIRE(info.min_value == Approx(-12.0));
        REQUIRE(info.max_value == Approx(12.0));
        REQUIRE(info.default_value == Approx(0.0));
    }

    SECTION("plain param flags") {
        REQUIRE(clap_params->get_info(plugin.clapPlugin(), 0, &info));
        REQUIRE(info.flags == CLAP_PARAM_IS_AUTOMATABLE);
    }

    SECTION("stepped hidden param flags") {
        REQUIRE(clap_params->get_info(plugin.clapPlugin(), 1, &info));
        REQUIRE(info.flags == (CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_HIDDEN | CLAP_PARAM_IS_AUTOMATABLE));
    }

    SECTION("cookies non-null and distinct") {
        clap_param_info_t other{};
        REQUIRE(clap_params->get_info(plugin.clapPlugin(), 0, &info));
        REQUIRE(clap_params->get_info(plugin.clapPlugin(), 1, &other));
        REQUIRE(info.cookie != nullptr);
        REQUIRE(other.cookie != nullptr);
        REQUIRE(info.cookie != other.cookie);
    }

    SECTION("out-of-range index fails") { REQUIRE_FALSE(clap_params->get_info(plugin.clapPlugin(), 2, &info)); }

    SECTION("count matches external params") { REQUIRE(clap_params->count(plugin.clapPlugin()) == 2); }

    SECTION("long name is truncated and null-terminated") {
        TestPlugin fresh;
        auto config = makeConfig("long", 0.5f);
        config.name = std::string(300, 'x');
        fresh.params.registerParam(config);
        clap_param_info_t n{};
        std::memset(n.name, 'z', sizeof(n.name));
        REQUIRE(clapParams(fresh)->get_info(fresh.clapPlugin(), 0, &n));
        REQUIRE(n.name[CLAP_NAME_SIZE - 1] == '\0');
        REQUIRE(std::strlen(n.name) == CLAP_NAME_SIZE - 1);
    }
}

TEST_CASE("ParamsExtension internal vs external enumeration", "[params][internal]") {
    TestPlugin plugin;
    plugin.params.registerParam(makeConfig("a", 0.5f));
    auto internal = makeConfig("sec", 0.25f);
    internal.is_internal = true;
    plugin.params.registerParam(internal);
    plugin.params.registerParam(makeConfig("b", 0.75f));

    auto* clap_params = clapParams(plugin);
    REQUIRE(clap_params->count(plugin.clapPlugin()) == 2);

    clap_param_info_t info{};
    REQUIRE(clap_params->get_info(plugin.clapPlugin(), 0, &info));
    REQUIRE(info.id == plugin.params.getInfo("a").clapId);
    REQUIRE(clap_params->get_info(plugin.clapPlugin(), 1, &info));
    REQUIRE(info.id == plugin.params.getInfo("b").clapId);
    REQUIRE_FALSE(clap_params->get_info(plugin.clapPlugin(), 2, &info));

    REQUIRE(plugin.params.getInfo("sec").internal);
    REQUIRE(plugin.params.getHandle("sec").getValue() == 0.25f);

    auto all = plugin.params.getAllParameters();
    REQUIRE(all.size() == 3);
    REQUIRE(all[1].stringId == "sec");
}

TEST_CASE("ParamsExtension CLAP id stability and module dependence", "[params][id]") {
    SECTION("same registration yields identical ids across instances") {
        ParamsExtension p1{4};
        p1.registerParam(makeConfig("a", 0.5f));
        ParamsExtension p2{4};
        p2.registerParam(makeConfig("a", 0.5f));
        REQUIRE(p1.getInfo("a").clapId == p2.getInfo("a").clapId);
    }

    SECTION("module changes the id") {
        ParamsExtension p1{4};
        p1.registerParam(makeConfig("cutoff", 0.5f));
        ParamsExtension p2{4};
        auto config = makeConfig("cutoff", 0.5f);
        config.module = "Filter";
        p2.registerParam(config);
        REQUIRE(p1.getInfo("cutoff").clapId != p2.getInfo("cutoff").clapId);
    }
}

TEST_CASE("ParamsExtension value access and get_value callback", "[params][value]") {
    TestPlugin plugin;
    plugin.params.registerParam(makeConfig("a", 0.5f));
    plugin.params.registerParam(makeConfig("b", 0.25f));

    REQUIRE(plugin.params.getHandle("a").getValue() == 0.5f);
    REQUIRE(plugin.params.getInfo("a").getValue() == 0.5f);

    auto* clap_params = clapParams(plugin);
    double value = 0.0;
    REQUIRE(clap_params->get_value(plugin.clapPlugin(), plugin.params.getInfo("b").clapId, &value));
    REQUIRE(value == Approx(0.25));

    SECTION("unknown id fails") { REQUIRE_FALSE(clap_params->get_value(plugin.clapPlugin(), 999999u, &value)); }
}

TEST_CASE("ParamsExtension coerces stepped defaults during registration", "[params][stepped][info]") {
    TestPlugin plugin;
    plugin.params.registerParam(rangedConfig("positive", -10.0f, 10.0f, 2.8f, true));
    plugin.params.registerParam(rangedConfig("negative", -10.0f, 10.0f, -2.8f, true));

    REQUIRE(plugin.params.getInfo("positive").defaultValue == 2.0f);
    REQUIRE(plugin.params.getInfo("positive").getValue() == 2.0f);
    REQUIRE(plugin.params.getInfo("negative").defaultValue == -2.0f);
    REQUIRE(plugin.params.getInfo("negative").getValue() == -2.0f);

    clap_param_info_t info{};
    REQUIRE(clapParams(plugin)->get_info(plugin.clapPlugin(), 0, &info));
    REQUIRE(info.default_value == 2.0);
    REQUIRE(clapParams(plugin)->get_info(plugin.clapPlugin(), 1, &info));
    REQUIRE(info.default_value == -2.0);
}

TEST_CASE("ParamsExtension choice parameters derive and expose enum metadata", "[params][choices][info]") {
    TestPlugin plugin;
    auto config = choiceConfig("mode", 4.0f, 5.0f, {"Off", "2x", "4x"});
    config.scaling = ValueScaling::quadratic();
    plugin.params.registerParam(config);

    config.choices[0] = "Changed";
    config.choices.clear();

    const auto& parameter = plugin.params.getInfo("mode");
    REQUIRE(parameter.minValue == 4.0f);
    REQUIRE(parameter.maxValue == 6.0f);
    REQUIRE(parameter.defaultValue == 5.0f);
    REQUIRE(parameter.getValue() == 5.0f);
    REQUIRE(parameter.stepped);
    REQUIRE(parameter.isEnumerated());
    REQUIRE(parameter.choices().size() == 3);
    REQUIRE(parameter.choices()[0] == "Off");
    REQUIRE(parameter.choices()[2] == "4x");

    const auto& scale = plugin.params.getScaleInfoArray()[0];
    REQUIRE(scale.min == 4.0f);
    REQUIRE(scale.max == 6.0f);
    REQUIRE(scale.scaling.type == ValueScale::Linear);
    REQUIRE(plugin.params.getNormalizedAt(0) == Approx(0.5f));
    REQUIRE(plugin.params.fromNormalizedAt(0, 1.0f) == Approx(6.0f));

    plugin.params.registerParam({.string_id = "implicit_default", .choices = {"A", "B"}});
    REQUIRE(plugin.params.getInfo("implicit_default").defaultValue == 0.0f);

    clap_param_info_t info{};
    REQUIRE(clapParams(plugin)->get_info(plugin.clapPlugin(), 0, &info));
    REQUIRE(info.min_value == Approx(4.0));
    REQUIRE(info.max_value == Approx(6.0));
    REQUIRE(info.default_value == Approx(5.0));
    REQUIRE(info.flags == (CLAP_PARAM_IS_STEPPED | CLAP_PARAM_IS_ENUM | CLAP_PARAM_IS_AUTOMATABLE));
}

TEST_CASE("ParamsExtension choice text conversion is bidirectional", "[params][choices][text]") {
    TestPlugin plugin;
    plugin.params.registerParam(choiceConfig("mode", 4.0f, 5.0f, {"Off", "2x", "4x"}));
    const auto& parameter = plugin.params.getInfo("mode");

    SECTION("values map by offset after normal coercion") {
        REQUIRE(parameter.valueToText(4.0f) == "Off");
        REQUIRE(parameter.valueToText(5.9f) == "2x");
        REQUIRE(parameter.valueToText(-100.0f) == "Off");
        REQUIRE(parameter.valueToText(100.0f) == "4x");
        REQUIRE(parameter.valueToText(std::numeric_limits<float>::quiet_NaN()) == "2x");
    }

    SECTION("labels are matched before numeric fallback") {
        REQUIRE(parameter.textToValue("Off") == 4.0f);
        REQUIRE(parameter.textToValue("2x") == 5.0f);
        REQUIRE(parameter.textToValue(" 2x ") == 5.0f);
        REQUIRE(parameter.textToValue("4x") == 6.0f);
        REQUIRE(parameter.textToValue("5.9") == 5.0f);
        REQUIRE(parameter.textToValue("999") == 6.0f);
        REQUIRE_FALSE(parameter.textToValue("unknown").has_value());
    }

    SECTION("inconsistent public metadata cannot cause out-of-bounds choice access") {
        auto& mutable_parameter = plugin.params.getInfo("mode");
        mutable_parameter.maxValue = 10.0f;
        REQUIRE_FALSE(mutable_parameter.valueToText(10.0f).empty());
    }

    SECTION("CLAP callbacks use the generated converters") {
        auto* clap_params = clapParams(plugin);
        char text[32] = {};
        REQUIRE(clap_params->value_to_text(plugin.clapPlugin(), parameter.clapId, 6.0, text, sizeof(text)));
        REQUIRE(std::string(text) == "4x");

        double value = 0.0;
        REQUIRE(clap_params->text_to_value(plugin.clapPlugin(), parameter.clapId, "2x", &value));
        REQUIRE(value == Approx(5.0));
        REQUIRE(clap_params->text_to_value(plugin.clapPlugin(), parameter.clapId, "6", &value));
        REQUIRE(value == Approx(6.0));
    }
}

TEST_CASE("ParamsExtension custom converters override choice converters per direction",
          "[params][choices][text][custom]") {
    TestPlugin plugin;

    auto custom_format = choiceConfig("format", 0.0f, 0.0f, {"A", "B"});
    custom_format.value_to_text = [](float value, const ParamInfo&) {
        return value == 0.0f ? "Custom A" : "Custom B";
    };
    plugin.params.registerParam(custom_format);

    auto custom_parse = choiceConfig("parse", 0.0f, 0.0f, {"A", "B"});
    custom_parse.text_to_value = [](const std::string& text, const ParamInfo&) -> std::optional<float> {
        if (text == "custom") return 1.0f;
        return std::nullopt;
    };
    plugin.params.registerParam(custom_parse);

    const auto& formatted = plugin.params.getInfo("format");
    REQUIRE(formatted.valueToText(1.0f) == "Custom B");
    REQUIRE(formatted.textToValue("Custom B") == 1.0f);
    REQUIRE(formatted.textToValue("B") == 1.0f);
    REQUIRE(formatted.textToValue("1") == 1.0f);

    const auto& parsed = plugin.params.getInfo("parse");
    REQUIRE(parsed.valueToText(1.0f) == "B");
    REQUIRE(parsed.textToValue("custom") == 1.0f);
    REQUIRE_FALSE(parsed.textToValue("B").has_value());
    REQUIRE_FALSE(parsed.textToValue("1").has_value());
}

TEST_CASE("ParamsExtension rejects invalid choice definitions", "[params][choices][validation]") {
    const auto registerConfig = [](ParamConfig config) {
        TestPlugin plugin;
        plugin.params.registerParam(config);
    };

    SECTION("at least two choices are required") {
        REQUIRE_THROWS_AS(registerConfig(choiceConfig("mode", 0.0f, 0.0f, {"Only"})), std::invalid_argument);
    }

    SECTION("labels must be nonblank, representable C strings, and unique") {
        REQUIRE_THROWS_AS(registerConfig(choiceConfig("empty", 0.0f, 0.0f, {"A", ""})),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(registerConfig(choiceConfig("blank", 0.0f, 0.0f, {"A", " \t"})),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(
            registerConfig(choiceConfig("nul", 0.0f, 0.0f, {"A", std::string("B\0C", 3)})),
            std::invalid_argument);
        REQUIRE_THROWS_AS(registerConfig(choiceConfig("duplicate", 0.0f, 0.0f, {"A", "A"})),
                          std::invalid_argument);
    }

    SECTION("minimum must be integral and defaults must be finite and in range") {
        REQUIRE_THROWS_AS(registerConfig(choiceConfig("minimum", 0.5f, 1.0f, {"A", "B"})),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(registerConfig(choiceConfig("nan_min", std::numeric_limits<float>::quiet_NaN(), 0.0f,
                                                      {"A", "B"})),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(registerConfig(choiceConfig("nan_default", 0.0f,
                                                      std::numeric_limits<float>::quiet_NaN(), {"A", "B"})),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(registerConfig(choiceConfig("outside", 4.0f, 6.0f, {"A", "B"})),
                          std::invalid_argument);
    }

    SECTION("custom formatters must produce valid unique labels") {
        auto blank = choiceConfig("blank_converter", 0.0f, 0.0f, {"A", "B"});
        blank.value_to_text = [](float value, const ParamInfo&) { return value == 0.0f ? "A" : ""; };
        REQUIRE_THROWS_AS(registerConfig(std::move(blank)), std::invalid_argument);

        auto duplicate = choiceConfig("duplicate_converter", 0.0f, 0.0f, {"A", "B"});
        duplicate.value_to_text = [](float, const ParamInfo&) { return "Same"; };
        REQUIRE_THROWS_AS(registerConfig(std::move(duplicate)), std::invalid_argument);

        auto ambiguous = choiceConfig("ambiguous_converter", 0.0f, 0.0f, {"A", "B"});
        ambiguous.value_to_text = [](float value, const ParamInfo&) { return value == 0.0f ? "B" : "C"; };
        REQUIRE_THROWS_AS(registerConfig(std::move(ambiguous)), std::invalid_argument);
    }

    SECTION("failed formatter validation leaves the next registration slot clean") {
        TestPlugin plugin;
        auto throwing = choiceConfig("throwing", 0.0f, 0.0f, {"A", "B"});
        throwing.value_to_text = [](float, const ParamInfo&) -> std::string {
            throw std::runtime_error("formatter failure");
        };
        REQUIRE_THROWS_AS(plugin.params.registerParam(throwing), std::runtime_error);
        REQUIRE(clapParams(plugin)->count(plugin.clapPlugin()) == 0);

        plugin.params.registerParam(choiceConfig("valid", 0.0f, 0.0f, {"A", "B"}));
        REQUIRE(clapParams(plugin)->count(plugin.clapPlugin()) == 1);
        REQUIRE(plugin.params.getInfo("valid").valueToText(1.0f) == "B");
    }
}

TEST_CASE("ParamsExtension choice indices round-trip through JSON state", "[params][choices][state]") {
    TestPlugin source;
    source.params.registerParam(choiceConfig("mode", 4.0f, 4.0f, {"Off", "2x", "4x"}));
    source.params.getInfo("mode").setValueSilently(6.0f);

    json state;
    REQUIRE(source.params.saveToJson(state));

    TestPlugin restored;
    restored.params.registerParam(choiceConfig("mode", 4.0f, 4.0f, {"Off", "2x", "4x"}));
    REQUIRE(restored.params.loadFromJson(state));
    REQUIRE(restored.params.getInfo("mode").getValue() == 6.0f);
    REQUIRE(restored.params.getInfo("mode").valueToText(6.0f) == "4x");
}

TEST_CASE("ParamsExtension default value_to_text formatting", "[params][text]") {
    TestPlugin plugin;
    plugin.params.registerParam(rangedConfig("f", -100000.0f, 100000.0f, 0.0f));
    plugin.params.registerParam(rangedConfig("s", -10.0f, 10.0f, 0.0f, true));
    auto with_unit = makeConfig("fu", 0.5f);
    with_unit.unit = "Hz";
    plugin.params.registerParam(with_unit);

    const auto& f = plugin.params.getInfo("f");
    const auto& s = plugin.params.getInfo("s");

    SECTION("stepped values render as integers") {
        REQUIRE(s.valueToText(2.8f) == "2");
        REQUIRE(s.valueToText(-2.8f) == "-2");
        REQUIRE(s.valueToText(20.0f) == "10");
        REQUIRE(s.valueToText(-20.0f) == "-10");
    }

    SECTION("decimal budget of five characters") {
        REQUIRE(f.valueToText(0.25f) == "0.25");
        REQUIRE(f.valueToText(3.14159f) == "3.14");
        REQUIRE(f.valueToText(123.4f) == "123.4");
        REQUIRE(f.valueToText(12345.0f) == "12345");
        REQUIRE(f.valueToText(1234.2f) == "1234");
        REQUIRE(f.valueToText(-0.5f) == "-0.50");
        REQUIRE(f.valueToText(-12.5f) == "-12.5");
    }

    SECTION("unit appended without space") { REQUIRE(plugin.params.getInfo("fu").valueToText(0.25f) == "0.25Hz"); }
}

TEST_CASE("ParamsExtension default text_to_value parsing", "[params][text]") {
    TestPlugin plugin;
    plugin.params.registerParam(rangedConfig("wide", -1000.0f, 1000.0f, 0.0f));
    plugin.params.registerParam(makeConfig("unit01", 0.5f));
    plugin.params.registerParam(rangedConfig("st", -3.5f, 3.5f, 0.0f, true));

    const auto& wide = plugin.params.getInfo("wide");

    SECTION("numbers with surrounding text") {
        REQUIRE(wide.textToValue("123.4").value() == Approx(123.4f));
        REQUIRE(wide.textToValue("50Hz").value() == Approx(50.0f));
        REQUIRE(wide.textToValue(" 100 ms").value() == Approx(100.0f));
        REQUIRE(wide.textToValue("-45.6dB").value() == Approx(-45.6f));
    }

    SECTION("clamped to the parameter range") {
        const auto& unit01 = plugin.params.getInfo("unit01");
        REQUIRE(unit01.textToValue("-45.6dB").value() == Approx(0.0f));
        REQUIRE(unit01.textToValue("999").value() == Approx(1.0f));
    }

    SECTION("no parseable number") {
        REQUIRE_FALSE(wide.textToValue("abc").has_value());
        REQUIRE_FALSE(wide.textToValue("").has_value());
        REQUIRE_FALSE(wide.textToValue("Hz").has_value());
    }

    SECTION("stepped values clamp before truncating toward zero") {
        const auto& stepped = plugin.params.getInfo("st");
        REQUIRE(stepped.textToValue("2.8").value() == Approx(2.0f));
        REQUIRE(stepped.textToValue("-2.8").value() == Approx(-2.0f));
        REQUIRE(stepped.textToValue("99").value() == Approx(3.0f));
        REQUIRE(stepped.textToValue("-99").value() == Approx(-3.0f));
    }
}

TEST_CASE("ParamsExtension clap text callbacks", "[params][text][clap]") {
    TestPlugin plugin;
    plugin.params.registerParam(makeConfig("f", 0.5f));
    const clap_id id = plugin.params.getInfo("f").clapId;
    auto* clap_params = clapParams(plugin);
    char buffer[32] = {};

    SECTION("value_to_text formats into the buffer") {
        REQUIRE(clap_params->value_to_text(plugin.clapPlugin(), id, 0.25, buffer, sizeof(buffer)));
        REQUIRE(std::string(buffer) == "0.25");
    }

    SECTION("small buffer truncates safely") {
        char small[3] = {'z', 'z', 'z'};
        REQUIRE(clap_params->value_to_text(plugin.clapPlugin(), id, 0.25, small, sizeof(small)));
        REQUIRE(std::string(small) == "0.");
    }

    SECTION("zero capacity fails") { REQUIRE_FALSE(clap_params->value_to_text(plugin.clapPlugin(), id, 0.25, buffer, 0)); }

    SECTION("unknown id fails both directions") {
        double out = 0.0;
        REQUIRE_FALSE(clap_params->value_to_text(plugin.clapPlugin(), 999999u, 0.5, buffer, sizeof(buffer)));
        REQUIRE_FALSE(clap_params->text_to_value(plugin.clapPlugin(), 999999u, "1", &out));
    }

    SECTION("text_to_value parses and rejects") {
        double out = 0.0;
        REQUIRE(clap_params->text_to_value(plugin.clapPlugin(), id, "0.5", &out));
        REQUIRE(out == Approx(0.5));
        REQUIRE_FALSE(clap_params->text_to_value(plugin.clapPlugin(), id, "abc", &out));
    }
}

TEST_CASE("ParamsExtension custom converters", "[params][text][custom]") {
    TestPlugin plugin;
    auto config = makeConfig("x", 0.5f);
    config.value_to_text = [](float value, const ParamInfo&) {
        return std::string("V=") + std::to_string(static_cast<int>(value * 100.0f));
    };
    config.text_to_value = [](const std::string& text, const ParamInfo&) -> std::optional<float> {
        if (text == "max") return 1.0f;
        return std::nullopt;
    };
    plugin.params.registerParam(config);

    const auto& info = plugin.params.getInfo("x");
    REQUIRE(info.valueToText(0.5f) == "V=50");
    REQUIRE(info.textToValue("max").value() == 1.0f);
    REQUIRE_FALSE(info.textToValue("nope").has_value());

    auto* clap_params = clapParams(plugin);
    char buffer[32] = {};
    REQUIRE(clap_params->value_to_text(plugin.clapPlugin(), info.clapId, 0.5, buffer, sizeof(buffer)));
    REQUIRE(std::string(buffer) == "V=50");
    double out = 0.0;
    REQUIRE(clap_params->text_to_value(plugin.clapPlugin(), info.clapId, "max", &out));
    REQUIRE(out == 1.0);
    REQUIRE_FALSE(clap_params->text_to_value(plugin.clapPlugin(), info.clapId, "nope", &out));
}

TEST_CASE("ParamsExtension coerces stepped values around custom converters", "[params][stepped][text][custom]") {
    TestPlugin plugin;
    std::vector<float> formatted_values;
    auto config = rangedConfig("stepped", -3.5f, 3.5f, 0.0f, true);
    config.value_to_text = [&](float value, const ParamInfo&) {
        formatted_values.push_back(value);
        return std::to_string(static_cast<int>(value));
    };
    config.text_to_value = [](const std::string& text, const ParamInfo&) -> std::optional<float> {
        if (text == "positive") return 2.8f;
        if (text == "negative") return -2.8f;
        if (text == "high") return 99.0f;
        if (text == "low") return -99.0f;
        return std::nullopt;
    };
    plugin.params.registerParam(config);

    const auto& info = plugin.params.getInfo("stepped");
    REQUIRE(info.valueToText(2.8f) == "2");
    REQUIRE(info.valueToText(-2.8f) == "-2");
    REQUIRE(formatted_values == std::vector<float>{2.0f, -2.0f});

    REQUIRE(info.textToValue("positive") == 2.0f);
    REQUIRE(info.textToValue("negative") == -2.0f);
    REQUIRE(info.textToValue("high") == 3.0f);
    REQUIRE(info.textToValue("low") == -3.0f);
    REQUIRE_FALSE(info.textToValue("unknown").has_value());
}

TEST_CASE("ParamsExtension processEvents inbound", "[params][process]") {
    TestPlugin plugin;
    plugin.params.registerParam(rangedConfig("s", -3.5f, 3.5f, 0.0f, true));
    plugin.params.registerParam(makeConfig("p", 0.5f));

    SECTION("stepped event values clamp before truncating and queue the coerced values") {
        ParamMessageQueue queue;
        plugin.params.setMessageQueue(&queue);
        const clap_id id = plugin.params.getInfo("s").clapId;
        EventList list;
        list.events.push_back(makeEvent(id, nullptr, 2.8));
        list.events.push_back(makeEvent(id, nullptr, -2.8));
        list.events.push_back(makeEvent(id, nullptr, 99.0));
        list.events.push_back(makeEvent(id, nullptr, -99.0));
        plugin.params.processEvents(&list.in, nullptr);

        REQUIRE(plugin.params.getInfo("s").getValue() == -3.0f);
        ParamMessageQueue::Message message{};
        for (const float expected : {2.0f, -2.0f, 3.0f, -3.0f}) {
            REQUIRE(queue.toUi().try_dequeue(message));
            REQUIRE(message.type == ParamMessageQueue::PARAM_VALUE);
            REQUIRE(message.index == 0);
            REQUIRE(message.value == expected);
        }
        REQUIRE_FALSE(queue.toUi().try_dequeue(message));
    }

    SECTION("host changes are forwarded to the UI queue") {
        ParamMessageQueue queue;
        plugin.params.setMessageQueue(&queue);
        const clap_id id = plugin.params.getInfo("p").clapId;
        EventList list;
        list.events.push_back(makeEvent(id, nullptr, 0.3));
        plugin.params.processEvents(&list.in, nullptr);

        ParamMessageQueue::Message message{};
        REQUIRE(queue.toUi().try_dequeue(message));
        REQUIRE(message.type == ParamMessageQueue::PARAM_VALUE);
        REQUIRE(message.index == 1);
        REQUIRE(message.value == Approx(0.3f));
        REQUIRE_FALSE(queue.toUi().try_dequeue(message));
    }

    SECTION("non-param events are skipped") {
        EventList list;
        auto event = makeEvent(plugin.params.getInfo("p").clapId, nullptr, 0.9);
        event.header.type = CLAP_EVENT_NOTE_ON;
        list.events.push_back(event);
        plugin.params.processEvents(&list.in, nullptr);
        REQUIRE(plugin.params.getInfo("p").getValue() == 0.5f);
    }

    SECTION("null event lists are safe") {
        plugin.params.processEvents(nullptr, nullptr);
        REQUIRE(plugin.params.getInfo("p").getValue() == 0.5f);
    }
}

TEST_CASE("ParamsExtension processEvents outbound", "[params][process][out]") {
    TestPlugin plugin;
    plugin.params.registerParam(makeConfig("p", 0.5f));
    const clap_id id = plugin.params.getInfo("p").clapId;

    ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    OutList out;

    SECTION("param value event fields") {
        queue.toAudio().enqueue({ParamMessageQueue::PARAM_VALUE, 0, 0.7f});
        plugin.params.processEvents(nullptr, &out.out);

        REQUIRE(out.values.size() == 1);
        const auto& event = out.values[0];
        REQUIRE(event.header.size == sizeof(clap_event_param_value_t));
        REQUIRE(event.header.time == 0);
        REQUIRE(event.header.space_id == CLAP_CORE_EVENT_SPACE_ID);
        REQUIRE(event.header.type == CLAP_EVENT_PARAM_VALUE);
        REQUIRE(event.header.flags == 0);
        REQUIRE(event.param_id == id);
        REQUIRE(event.cookie == nullptr);
        REQUIRE(event.note_id == -1);
        REQUIRE(event.port_index == -1);
        REQUIRE(event.channel == -1);
        REQUIRE(event.key == -1);
        REQUIRE(event.value == Approx(0.7));
    }

    SECTION("gesture events") {
        queue.toAudio().enqueue({ParamMessageQueue::BEGIN_GESTURE, 0, 0.0f});
        queue.toAudio().enqueue({ParamMessageQueue::END_GESTURE, 0, 0.0f});
        plugin.params.processEvents(nullptr, &out.out);

        REQUIRE(out.gestures.size() == 2);
        REQUIRE(out.gestures[0].header.type == CLAP_EVENT_PARAM_GESTURE_BEGIN);
        REQUIRE(out.gestures[0].header.size == sizeof(clap_event_param_gesture_t));
        REQUIRE(out.gestures[0].param_id == id);
        REQUIRE(out.gestures[1].header.type == CLAP_EVENT_PARAM_GESTURE_END);
        REQUIRE(out.gestures[1].param_id == id);
    }

    SECTION("ordering preserved") {
        queue.toAudio().enqueue({ParamMessageQueue::PARAM_VALUE, 0, 0.1f});
        queue.toAudio().enqueue({ParamMessageQueue::BEGIN_GESTURE, 0, 0.0f});
        queue.toAudio().enqueue({ParamMessageQueue::PARAM_VALUE, 0, 0.2f});
        queue.toAudio().enqueue({ParamMessageQueue::END_GESTURE, 0, 0.0f});
        plugin.params.processEvents(nullptr, &out.out);

        REQUIRE(out.types == std::vector<uint16_t>{CLAP_EVENT_PARAM_VALUE, CLAP_EVENT_PARAM_GESTURE_BEGIN,
                                                   CLAP_EVENT_PARAM_VALUE, CLAP_EVENT_PARAM_GESTURE_END});
        REQUIRE(out.values[0].value == Approx(0.1));
        REQUIRE(out.values[1].value == Approx(0.2));
    }

    SECTION("empty queue pushes nothing") {
        plugin.params.processEvents(nullptr, &out.out);
        REQUIRE(out.types.empty());
    }
}

TEST_CASE("ParamsExtension sends no host events for internal parameters", "[params][process][out]") {
    TestPlugin plugin;
    auto hidden = makeConfig("hidden", 0.5f);
    hidden.is_internal = true;
    plugin.params.registerParam(hidden);
    plugin.params.registerParam(makeConfig("visible", 0.5f));

    ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    OutList out;

    queue.toAudio().enqueue({ParamMessageQueue::BEGIN_GESTURE, 0, 0.0f});
    queue.toAudio().enqueue({ParamMessageQueue::PARAM_VALUE, 0, 0.3f});
    queue.toAudio().enqueue({ParamMessageQueue::END_GESTURE, 0, 0.0f});
    queue.toAudio().enqueue({ParamMessageQueue::PARAM_VALUE, 1, 0.6f});
    plugin.params.processEvents(nullptr, &out.out);

    REQUIRE(out.types == std::vector<uint16_t>{CLAP_EVENT_PARAM_VALUE});
    REQUIRE(out.values[0].param_id == plugin.params.getInfo("visible").clapId);
}

TEST_CASE("ParamsExtension flush drives both directions", "[params][process][flush]") {
    TestPlugin plugin;
    plugin.params.registerParam(makeConfig("p", 0.5f));
    plugin.params.registerParam(makeConfig("q2", 0.5f));
    const clap_id id_p = plugin.params.getInfo("p").clapId;
    const clap_id id_q2 = plugin.params.getInfo("q2").clapId;

    ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    queue.toAudio().enqueue({ParamMessageQueue::PARAM_VALUE, 1, 0.8f});

    EventList list;
    list.events.push_back(makeEvent(id_p, nullptr, 0.4));
    OutList out;

    clapParams(plugin)->flush(plugin.clapPlugin(), &list.in, &out.out);

    REQUIRE(plugin.params.getInfo("p").getValue() == 0.4f);

    ParamMessageQueue::Message message{};
    REQUIRE(queue.toUi().try_dequeue(message));
    REQUIRE(message.index == 0);
    REQUIRE(message.value == Approx(0.4f));

    REQUIRE(out.values.size() == 1);
    REQUIRE(out.values[0].param_id == id_q2);
    REQUIRE(out.values[0].value == Approx(0.8));
}

TEST_CASE("ParamInfo UI methods notify host and queue", "[params][ui][host]") {
    FakeHost fake;
    TestPlugin plugin(&fake.host);
    plugin.params.registerParam(makeConfig("p", 0.5f));

    ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    REQUIRE(plugin.clapPlugin()->init(plugin.clapPlugin()));

    const auto& info = plugin.params.getInfo("p");
    ParamMessageQueue::Message message{};

    SECTION("setValueNotifyingHost stores, signals, queues, and flushes") {
        float seen = -1.0f;
        info.on_value_changed.connect([&](float value) { seen = value; });
        info.setValueNotifyingHost(0.75f);

        REQUIRE(info.getValue() == 0.75f);
        REQUIRE(seen == 0.75f);
        REQUIRE(queue.toAudio().try_dequeue(message));
        REQUIRE(message.type == ParamMessageQueue::PARAM_VALUE);
        REQUIRE(message.index == 0);
        REQUIRE(message.value == 0.75f);
        REQUIRE_FALSE(queue.toAudio().try_dequeue(message));
        REQUIRE(fake.flush_count == 1);
    }

    SECTION("setValueNotifyingHost clamps everywhere") {
        float seen = -1.0f;
        info.on_value_changed.connect([&](float value) { seen = value; });
        info.setValueNotifyingHost(1.5f);

        REQUIRE(info.getValue() == 1.0f);
        REQUIRE(seen == 1.0f);
        REQUIRE(queue.toAudio().try_dequeue(message));
        REQUIRE(message.value == 1.0f);
    }

    SECTION("setValueSilently stores without notifications") {
        float seen = -2.0f;
        info.on_value_changed.connect([&](float value) { seen = value; });
        info.setValueSilently(0.3f);

        REQUIRE(info.getValue() == 0.3f);
        REQUIRE(seen == -2.0f);
        REQUIRE_FALSE(queue.toAudio().try_dequeue(message));
        REQUIRE(fake.flush_count == 0);

        info.setValueSilently(2.0f);
        REQUIRE(info.getValue() == 1.0f);
    }

    SECTION("beginGesture queues and flushes") {
        info.beginGesture();
        REQUIRE(queue.toAudio().try_dequeue(message));
        REQUIRE(message.type == ParamMessageQueue::BEGIN_GESTURE);
        REQUIRE(message.index == 0);
        REQUIRE(fake.flush_count == 1);
    }

    SECTION("endGesture queues and flushes") {
        info.endGesture();
        REQUIRE(queue.toAudio().try_dequeue(message));
        REQUIRE(message.type == ParamMessageQueue::END_GESTURE);
        REQUIRE(message.index == 0);
        REQUIRE(fake.flush_count == 1);
    }
}

TEST_CASE("ParamInfo UI methods coerce stepped values before notifications", "[params][stepped][ui]") {
    FakeHost fake;
    TestPlugin plugin(&fake.host);
    plugin.params.registerParam(rangedConfig("stepped", -10.0f, 10.0f, 0.0f, true));
    const auto& info = plugin.params.getInfo("stepped");

    ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    REQUIRE(plugin.clapPlugin()->init(plugin.clapPlugin()));

    std::vector<float> seen;
    info.on_value_changed.connect([&](float value) { seen.push_back(value); });
    ParamMessageQueue::Message message{};

    SECTION("setValueNotifyingHost stores, signals, and queues coerced values") {
        info.setValueNotifyingHost(2.8f);
        info.setValueNotifyingHost(-2.8f);

        REQUIRE(info.getValue() == -2.0f);
        REQUIRE(seen == std::vector<float>{2.0f, -2.0f});
        for (const float expected : {2.0f, -2.0f}) {
            REQUIRE(queue.toAudio().try_dequeue(message));
            REQUIRE(message.type == ParamMessageQueue::PARAM_VALUE);
            REQUIRE(message.index == 0);
            REQUIRE(message.value == expected);
        }
        REQUIRE_FALSE(queue.toAudio().try_dequeue(message));
        REQUIRE(fake.flush_count == 2);
    }

    SECTION("setValueSilently stores coerced values without notifications") {
        info.setValueSilently(2.8f);
        REQUIRE(info.getValue() == 2.0f);
        info.setValueSilently(-2.8f);
        REQUIRE(info.getValue() == -2.0f);

        REQUIRE(seen.empty());
        REQUIRE_FALSE(queue.toAudio().try_dequeue(message));
        REQUIRE(fake.flush_count == 0);
    }
}

TEST_CASE("ParamsExtension host rescan", "[params][host]") {
    SECTION("forwards flags to the host") {
        FakeHost fake;
        TestPlugin plugin(&fake.host);
        plugin.params.registerParam(makeConfig("p", 0.5f));
        REQUIRE(plugin.clapPlugin()->init(plugin.clapPlugin()));

        plugin.params.rescan(CLAP_PARAM_RESCAN_VALUES);
        REQUIRE(fake.rescan_count == 1);
        REQUIRE(fake.last_rescan_flags == CLAP_PARAM_RESCAN_VALUES);

        plugin.params.rescan(CLAP_PARAM_RESCAN_ALL);
        REQUIRE(fake.rescan_count == 2);
        REQUIRE(fake.last_rescan_flags == CLAP_PARAM_RESCAN_ALL);
    }

    SECTION("safe no-op without a host") {
        TestPlugin plugin;
        plugin.params.registerParam(makeConfig("p", 0.5f));
        REQUIRE(plugin.clapPlugin()->init(plugin.clapPlugin()));
        plugin.params.rescan(CLAP_PARAM_RESCAN_ALL);
        REQUIRE(plugin.params.getInfo("p").getValue() == 0.5f);
    }
}

TEST_CASE("ParamsExtension normalization", "[params][scaling]") {
    TestPlugin plugin;
    auto quad = rangedConfig("q", 0.0f, 4.0f, 1.0f);
    quad.scaling = ValueScaling::quadratic();
    plugin.params.registerParam(quad);
    plugin.params.registerParam(rangedConfig("lin", 0.0f, 10.0f, 2.5f));

    const auto& q = plugin.params.getInfo("q");

    SECTION("quadratic ParamInfo math") {
        REQUIRE(q.toNormalized(1.0f) == Approx(0.5f));
        REQUIRE(q.toNormalized(4.0f) == Approx(1.0f));
        REQUIRE(q.toNormalized(0.0f) == Approx(0.0f));
        REQUIRE(q.fromNormalized(0.5f) == Approx(1.0f));
        REQUIRE(q.fromNormalized(q.toNormalized(3.0f)) == Approx(3.0f));
        REQUIRE(q.getNormalized() == Approx(0.5f));
    }

    SECTION("extension-level helpers agree with ParamInfo") {
        REQUIRE(plugin.params.getNormalizedAt(0) == Approx(q.getNormalized()));
        REQUIRE(plugin.params.fromNormalizedAt(0, 0.5f) == Approx(1.0f));
        REQUIRE(plugin.params.getNormalizedAt(1) == Approx(0.25f));
        REQUIRE(plugin.params.fromNormalizedAt(1, 0.25f) == Approx(2.5f));
    }

    SECTION("linear sanity") {
        const auto& lin = plugin.params.getInfo("lin");
        REQUIRE(lin.toNormalized(2.5f) == Approx(0.25f));
        REQUIRE(lin.fromNormalized(0.25f) == Approx(2.5f));
    }
}

TEST_CASE("ParamsExtension JSON save and load", "[params][json]") {
    TestPlugin plugin;
    plugin.params.registerParam(makeConfig("a", 0.5f));
    plugin.params.registerParam(makeConfig("b", 0.25f));
    const clap_id id_a = plugin.params.getInfo("a").clapId;

    SECTION("save writes id, value, and name in registration order") {
        plugin.params.getInfo("a").setValueSilently(0.7f);
        applause::json state;
        REQUIRE(plugin.params.saveToJson(state));
        REQUIRE(state.is_array());
        REQUIRE(state.size() == 2);
        REQUIRE(state[0]["id"] == id_a);
        REQUIRE(state[0]["value"].get<float>() == Approx(0.7f));
        REQUIRE(state[0]["name"] == "a");
        REQUIRE(state[1]["id"] == plugin.params.getInfo("b").clapId);
    }

    SECTION("round-trip restores values across identically registered plugins") {
        plugin.params.getInfo("a").setValueSilently(0.7f);
        applause::json state;
        REQUIRE(plugin.params.saveToJson(state));

        TestPlugin other;
        other.params.registerParam(makeConfig("a", 0.5f));
        other.params.registerParam(makeConfig("b", 0.25f));
        REQUIRE(other.params.loadFromJson(state));
        REQUIRE(other.params.getInfo("a").getValue() == Approx(0.7f));
        REQUIRE(other.params.getInfo("b").getValue() == 0.25f);
    }

    SECTION("unknown ids are skipped") {
        applause::json state = applause::json::array();
        state.push_back({{"id", id_a}, {"value", 0.9f}});
        state.push_back({{"id", 999999u}, {"value", 0.1f}});
        REQUIRE(plugin.params.loadFromJson(state));
        REQUIRE(plugin.params.getInfo("a").getValue() == Approx(0.9f));
    }

    SECTION("non-array state is tolerated") {
        applause::json state = 42;
        REQUIRE(plugin.params.loadFromJson(state));
        REQUIRE(plugin.params.getInfo("a").getValue() == 0.5f);
    }

    SECTION("non-object entries are skipped") {
        applause::json state = applause::json::array();
        state.push_back(42);
        state.push_back({{"id", id_a}, {"value", 0.33f}});
        REQUIRE(plugin.params.loadFromJson(state));
        REQUIRE(plugin.params.getInfo("a").getValue() == Approx(0.33f));
    }

    SECTION("loaded values are clamped to the parameter range") {
        applause::json state = applause::json::array();
        state.push_back({{"id", id_a}, {"value", 2.0f}});
        REQUIRE(plugin.params.loadFromJson(state));
        REQUIRE(plugin.params.getInfo("a").getValue() == 1.0f);
    }

    SECTION("loading notifies listeners directly instead of queueing") {
        ParamMessageQueue queue;
        plugin.params.setMessageQueue(&queue);
        float seen = -1.0f;
        plugin.params.getInfo("a").on_value_changed.connect([&](float value) { seen = value; });
        applause::json state = applause::json::array();
        state.push_back({{"id", id_a}, {"value", 0.42f}});
        REQUIRE(plugin.params.loadFromJson(state));

        REQUIRE(seen == Approx(0.42f));
        ParamMessageQueue::Message message{};
        REQUIRE_FALSE(queue.toUi().try_dequeue(message));
    }

    SECTION("stepped values are coerced before storage and notification") {
        TestPlugin stepped;
        stepped.params.registerParam(rangedConfig("positive", -10.0f, 10.0f, 0.0f, true));
        stepped.params.registerParam(rangedConfig("negative", -10.0f, 10.0f, 0.0f, true));
        std::vector<float> seen;
        for (const char* id : {"positive", "negative"})
            stepped.params.getInfo(id).on_value_changed.connect([&](float value) { seen.push_back(value); });

        applause::json state = applause::json::array();
        state.push_back({{"id", stepped.params.getInfo("positive").clapId}, {"value", 2.8f}});
        state.push_back({{"id", stepped.params.getInfo("negative").clapId}, {"value", -2.8f}});
        REQUIRE(stepped.params.loadFromJson(state));

        REQUIRE(stepped.params.getInfo("positive").getValue() == 2.0f);
        REQUIRE(stepped.params.getInfo("negative").getValue() == -2.0f);
        REQUIRE(seen == std::vector<float>{2.0f, -2.0f});
    }
}

TEST_CASE("Dynamic parameters activate into the pool and deactivate", "[params][dynamic]") {
    DynamicPlugin plugin;
    plugin.params.registerParam(makeConfig("gain", 0.5f));

    std::vector<std::pair<uint32_t, bool>> changes;
    std::string id_at_deactivation;
    plugin.params.on_dynamic_param_changed.connect([&](uint32_t index, bool active) {
        changes.emplace_back(index, active);
        if (!active) id_at_deactivation = plugin.params.getAllParameters()[index].stringId;
    });

    REQUIRE(plugin.params.activateDynamicParam(rangedConfig("fx/1/rate", 0.1f, 10.0f, 1.0f)));
    const auto& rate = plugin.params.getInfo("fx/1/rate");

    SECTION("an active dynamic parameter behaves like an internal one") {
        REQUIRE(rate.getValue() == 1.0f);
        REQUIRE(plugin.params.getHandle("fx/1/rate").getValue() == 1.0f);
        REQUIRE(rate.internal);
        REQUIRE(rate.maxValue == 10.0f);
        REQUIRE(clapParams(plugin)->count(plugin.clapPlugin()) == 1);
        REQUIRE(changes == std::vector<std::pair<uint32_t, bool>>{{0, true}});
    }

    SECTION("static parameters are stored after the pool, and every pool entry is internal") {
        const auto params = plugin.params.getAllParameters();
        REQUIRE(params.size() == 4);
        REQUIRE(params[3].stringId == "gain");
        REQUIRE(params[1].internal);
        REQUIRE(params[1].stringId.empty());
    }

    SECTION("string ids must be unique across static and dynamic parameters") {
        REQUIRE_FALSE(plugin.params.activateDynamicParam(makeConfig("fx/1/rate", 0.5f)));
        REQUIRE_FALSE(plugin.params.activateDynamicParam(makeConfig("gain", 0.5f)));
    }

    SECTION("activation fails once the pool is full") {
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("fx/1/mix", 0.5f)));
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("fx/1/depth", 0.5f)));
        REQUIRE_FALSE(plugin.params.activateDynamicParam(makeConfig("fx/2/mix", 0.5f)));
    }

    SECTION("deactivation fires the signal while the parameter is still readable, then frees the entry") {
        plugin.params.deactivateDynamicParam("fx/1/rate");
        REQUIRE(changes.back() == std::pair<uint32_t, bool>{0, false});
        REQUIRE(id_at_deactivation == "fx/1/rate");
        REQUIRE(plugin.params.getAllParameters()[0].stringId.empty());
        REQUIRE(plugin.params.activateDynamicParam(rangedConfig("fx/1/rate", 0.0f, 2.0f, 2.0f)));
        REQUIRE(plugin.params.getInfo("fx/1/rate").getValue() == 2.0f);
    }

    SECTION("freed entries are reused oldest first") {
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("b", 0.5f)));
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("c", 0.5f)));
        plugin.params.deactivateDynamicParam("b");
        plugin.params.deactivateDynamicParam("fx/1/rate");
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("d", 0.5f)));
        REQUIRE(changes.back() == std::pair<uint32_t, bool>{1, true});
    }

    SECTION("an invalid config throws without taking an entry") {
        REQUIRE_THROWS_AS(plugin.params.activateDynamicParam(choiceConfig("bad", 0.0f, 0.0f, {"only"})),
                          std::invalid_argument);
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("b", 0.5f)));
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("c", 0.5f)));
    }
}

TEST_CASE("Dynamic parameters save and load by string id", "[params][dynamic][json]") {
    DynamicPlugin plugin;
    plugin.params.registerParam(makeConfig("gain", 0.5f));
    REQUIRE(plugin.params.activateDynamicParam(rangedConfig("fx/1/rate", 0.1f, 10.0f, 1.0f)));
    plugin.params.getInfo("fx/1/rate").setValueSilently(5.0f);
    plugin.params.getInfo("gain").setValueSilently(0.25f);

    applause::json state;
    REQUIRE(plugin.params.saveToJson(state));
    REQUIRE(state.size() == 2);
    REQUIRE(state[0]["id"] == plugin.params.getInfo("gain").clapId);
    REQUIRE(state[1]["string_id"] == "fx/1/rate");
    REQUIRE_FALSE(state[1].contains("id"));

    SECTION("values land on the parameter with the same string id, whatever its pool entry") {
        DynamicPlugin other;
        other.params.registerParam(makeConfig("gain", 0.5f));
        REQUIRE(other.params.activateDynamicParam(makeConfig("fx/2/mix", 0.5f)));
        REQUIRE(other.params.activateDynamicParam(rangedConfig("fx/1/rate", 0.1f, 10.0f, 1.0f)));
        REQUIRE(other.params.loadFromJson(state));
        REQUIRE(other.params.getInfo("fx/1/rate").getValue() == 5.0f);
        REQUIRE(other.params.getInfo("fx/2/mix").getValue() == 0.5f);
        REQUIRE(other.params.getInfo("gain").getValue() == 0.25f);
    }

    SECTION("entries for parameters that aren't active are skipped") {
        DynamicPlugin other;
        other.params.registerParam(makeConfig("gain", 0.5f));
        REQUIRE(other.params.loadFromJson(state));
        REQUIRE(other.params.getInfo("gain").getValue() == 0.25f);
    }
}

TEST_CASE("Automation slots are listed after the static parameters", "[params][slots]") {
    SlotPlugin plugin;
    plugin.params.registerParam(makeConfig("gain", 0.5f));
    auto tab = makeConfig("tab", 0.0f);
    tab.is_internal = true;
    plugin.params.registerParam(tab);

    auto* clap_params = clapParams(plugin);
    REQUIRE(clap_params->count(plugin.clapPlugin()) == 3);

    clap_param_info_t slot{};
    REQUIRE(clap_params->get_info(plugin.clapPlugin(), 1, &slot));
    REQUIRE(std::string(slot.name) == "Unused 1");
    REQUIRE(std::string(slot.module).empty());
    REQUIRE(slot.min_value == 0.0);
    REQUIRE(slot.max_value == 1.0);
    REQUIRE(slot.default_value == 0.0);
    REQUIRE(slot.flags == CLAP_PARAM_IS_AUTOMATABLE);
    REQUIRE(slot.cookie != nullptr);
    REQUIRE(slot.id != plugin.params.getInfo("gain").clapId);

    clap_param_info_t second{};
    REQUIRE(clap_params->get_info(plugin.clapPlugin(), 2, &second));
    REQUIRE(std::string(second.name) == "Unused 2");
    REQUIRE(second.id != slot.id);
    REQUIRE_FALSE(clap_params->get_info(plugin.clapPlugin(), 3, &second));

    SlotPlugin other;
    clap_param_info_t other_slot{};
    REQUIRE(clapParams(other)->get_info(other.clapPlugin(), 0, &other_slot));
    REQUIRE(other_slot.id == slot.id);

    double value = -1.0;
    REQUIRE(clap_params->get_value(plugin.clapPlugin(), slot.id, &value));
    REQUIRE(value == 0.0);
    char text[64];
    REQUIRE(clap_params->value_to_text(plugin.clapPlugin(), slot.id, 0.5, text, sizeof(text)));
    REQUIRE(std::string(text) == "-");
    REQUIRE_FALSE(clap_params->text_to_value(plugin.clapPlugin(), slot.id, "0.5", &value));
}

TEST_CASE("setAutomatable binds a dynamic parameter to a slot", "[params][slots]") {
    FakeHost fake;
    SlotPlugin plugin(&fake.host);
    plugin.params.registerParam(makeConfig("gain", 0.5f));
    auto tab = makeConfig("tab", 0.0f);
    tab.is_internal = true;
    plugin.params.registerParam(tab);
    REQUIRE(plugin.clapPlugin()->init(plugin.clapPlugin()));

    auto rate_config = rangedConfig("fx/1/rate", 0.0f, 10.0f, 1.0f);
    rate_config.name = "Chorus Rate";
    rate_config.module = "FX 1";
    REQUIRE(plugin.params.activateDynamicParam(rate_config));
    const auto& rate = plugin.params.getInfo("fx/1/rate");
    auto* clap_params = clapParams(plugin);

    REQUIRE_FALSE(rate.isAutomatable());
    REQUIRE(rate.setAutomatable(true));
    REQUIRE(rate.isAutomatable());

    clap_param_info_t slot{};
    REQUIRE(clap_params->get_info(plugin.clapPlugin(), 1, &slot));
    REQUIRE(std::string(slot.name) == "Chorus Rate");
    REQUIRE(std::string(slot.module) == "FX 1");
    double value = -1.0;
    REQUIRE(clap_params->get_value(plugin.clapPlugin(), slot.id, &value));
    REQUIRE(value == Approx(0.1));
    char text[64];
    REQUIRE(clap_params->value_to_text(plugin.clapPlugin(), slot.id, 0.5, text, sizeof(text)));
    REQUIRE(std::string(text) == rate.valueToText(5.0f));
    REQUIRE(clap_params->text_to_value(plugin.clapPlugin(), slot.id, "2.5", &value));
    REQUIRE(value == Approx(0.25));

    SECTION("the host rescan waits for the main-thread callback and is coalesced") {
        REQUIRE(fake.callback_count == 1);
        REQUIRE(fake.rescan_count == 0);
        plugin.clapPlugin()->on_main_thread(plugin.clapPlugin());
        REQUIRE(fake.rescan_count == 1);
        REQUIRE(fake.last_rescan_flags ==
                (CLAP_PARAM_RESCAN_INFO | CLAP_PARAM_RESCAN_VALUES | CLAP_PARAM_RESCAN_TEXT));
        plugin.clapPlugin()->on_main_thread(plugin.clapPlugin());
        REQUIRE(fake.rescan_count == 1);
    }

    SECTION("turning automation off frees the slot") {
        REQUIRE(rate.setAutomatable(false));
        REQUIRE_FALSE(rate.isAutomatable());
        REQUIRE(clap_params->get_info(plugin.clapPlugin(), 1, &slot));
        REQUIRE(std::string(slot.name) == "Unused 1");
    }

    SECTION("deactivating an automated parameter frees its slot") {
        plugin.params.deactivateDynamicParam("fx/1/rate");
        REQUIRE(clap_params->get_info(plugin.clapPlugin(), 1, &slot));
        REQUIRE(std::string(slot.name) == "Unused 1");
    }

    SECTION("static parameters report their own automatability and can't borrow slots") {
        REQUIRE(plugin.params.getInfo("gain").isAutomatable());
        REQUIRE_FALSE(plugin.params.getInfo("tab").isAutomatable());
        REQUIRE_FALSE(plugin.params.getInfo("tab").setAutomatable(true));
    }

    SECTION("setAutomatable fails when every slot is taken") {
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("a", 0.5f)));
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("b", 0.5f)));
        REQUIRE(plugin.params.getInfo("a").setAutomatable(true));
        REQUIRE_FALSE(plugin.params.getInfo("b").setAutomatable(true));
    }
}

TEST_CASE("Slot events drive the bound dynamic parameter", "[params][slots][process]") {
    SlotPlugin plugin;
    REQUIRE(plugin.params.activateDynamicParam(rangedConfig("rate", 0.0f, 10.0f, 1.0f)));
    REQUIRE(plugin.params.activateDynamicParam(choiceConfig("mode", 0.0f, 0.0f, {"a", "b", "c", "d"})));
    const auto& rate = plugin.params.getInfo("rate");
    const auto& mode = plugin.params.getInfo("mode");
    REQUIRE(rate.setAutomatable(true));
    REQUIRE(mode.setAutomatable(true));

    auto* clap_params = clapParams(plugin);
    clap_param_info_t rate_slot{};
    clap_param_info_t mode_slot{};
    REQUIRE(clap_params->get_info(plugin.clapPlugin(), 0, &rate_slot));
    REQUIRE(clap_params->get_info(plugin.clapPlugin(), 1, &mode_slot));

    const auto send = [&](const clap_param_info_t& slot, double value, bool cookie = true) {
        EventList list;
        list.events.push_back(makeEvent(slot.id, cookie ? slot.cookie : nullptr, value));
        plugin.params.processEvents(&list.in, nullptr);
    };

    SECTION("continuous values map through the parameter's range") {
        ParamMessageQueue queue;
        plugin.params.setMessageQueue(&queue);
        send(rate_slot, 0.25);
        REQUIRE(rate.getValue() == Approx(2.5f));
        ParamMessageQueue::Message message{};
        REQUIRE(queue.toUi().try_dequeue(message));
        REQUIRE(message.index == 0);
        REQUIRE(message.value == Approx(2.5f));

        send(rate_slot, 0.75, false);
        REQUIRE(rate.getValue() == Approx(7.5f));
        send(rate_slot, std::numeric_limits<double>::quiet_NaN());
        REQUIRE(rate.getValue() == 0.0f);
    }

    SECTION("stepped values use equal bins in both directions") {
        const std::vector<std::pair<double, float>> cases{
            {0.0, 0.0f}, {0.24, 0.0f}, {0.26, 1.0f}, {0.74, 2.0f}, {0.99, 3.0f}, {1.0, 3.0f}};
        for (const auto& [normalized, expected] : cases) {
            send(mode_slot, normalized);
            REQUIRE(mode.getValue() == expected);
        }

        double value = -1.0;
        mode.setValueSilently(1.0f);
        REQUIRE(clap_params->get_value(plugin.clapPlugin(), mode_slot.id, &value));
        REQUIRE(value == Approx(1.0 / 3.0));
        mode.setValueSilently(3.0f);
        REQUIRE(clap_params->get_value(plugin.clapPlugin(), mode_slot.id, &value));
        REQUIRE(value == 1.0);
    }

    SECTION("events for an unused slot are dropped") {
        REQUIRE(rate.setAutomatable(false));
        send(rate_slot, 0.9);
        REQUIRE(rate.getValue() == 1.0f);
    }
}

TEST_CASE("UI edits of dynamic parameters reach the host only through a slot", "[params][slots][process][out]") {
    SlotPlugin plugin;
    REQUIRE(plugin.params.activateDynamicParam(rangedConfig("rate", 0.0f, 10.0f, 1.0f)));
    ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    OutList out;

    queue.toAudio().enqueue({ParamMessageQueue::PARAM_VALUE, 0, 5.0f});
    plugin.params.processEvents(nullptr, &out.out);
    REQUIRE(out.types.empty());

    REQUIRE(plugin.params.getInfo("rate").setAutomatable(true));
    clap_param_info_t slot{};
    REQUIRE(clapParams(plugin)->get_info(plugin.clapPlugin(), 0, &slot));
    queue.toAudio().enqueue({ParamMessageQueue::BEGIN_GESTURE, 0, 0.0f});
    queue.toAudio().enqueue({ParamMessageQueue::PARAM_VALUE, 0, 5.0f});
    queue.toAudio().enqueue({ParamMessageQueue::END_GESTURE, 0, 0.0f});
    plugin.params.processEvents(nullptr, &out.out);

    REQUIRE(out.types == std::vector<uint16_t>{CLAP_EVENT_PARAM_GESTURE_BEGIN, CLAP_EVENT_PARAM_VALUE,
                                               CLAP_EVENT_PARAM_GESTURE_END});
    REQUIRE(out.values[0].param_id == slot.id);
    REQUIRE(out.values[0].value == Approx(0.5));
    REQUIRE(out.gestures[0].param_id == slot.id);
    REQUIRE(out.gestures[1].param_id == slot.id);
}

TEST_CASE("Slot choice prefers the parameter's previous slot, then never-used slots", "[params][slots]") {
    SlotPlugin plugin(nullptr, 3);
    for (const char* id : {"a", "b", "c"}) REQUIRE(plugin.params.activateDynamicParam(makeConfig(id, 0.5f)));
    const auto slotName = [&](uint32_t slot) {
        clap_param_info_t info{};
        REQUIRE(clapParams(plugin)->get_info(plugin.clapPlugin(), slot, &info));
        return std::string(info.name);
    };
    const auto& a = plugin.params.getInfo("a");
    const auto& b = plugin.params.getInfo("b");
    const auto& c = plugin.params.getInfo("c");

    REQUIRE(a.setAutomatable(true));
    REQUIRE(slotName(0) == "a");
    REQUIRE(a.setAutomatable(false));
    REQUIRE(b.setAutomatable(true));
    REQUIRE(slotName(1) == "b");
    REQUIRE(a.setAutomatable(true));
    REQUIRE(slotName(0) == "a");
    REQUIRE(b.setAutomatable(false));
    REQUIRE(c.setAutomatable(true));
    REQUIRE(slotName(2) == "c");
}

TEST_CASE("A dynamic entry that held a slot is reused only after the audio thread moves on",
          "[params][slots][dynamic]") {
    FakeHost fake;
    SlotPlugin plugin(&fake.host);
    REQUIRE(plugin.clapPlugin()->init(plugin.clapPlugin()));
    REQUIRE(plugin.clapPlugin()->activate(plugin.clapPlugin(), 48000.0, 32, 512));
    for (const char* id : {"a", "b", "c"}) REQUIRE(plugin.params.activateDynamicParam(makeConfig(id, 0.5f)));
    REQUIRE(plugin.params.getInfo("a").setAutomatable(true));
    plugin.params.deactivateDynamicParam("a");
    REQUIRE(fake.flush_count == 1);

    SECTION("while active, the entry waits for a processEvents() call") {
        REQUIRE_FALSE(plugin.params.activateDynamicParam(makeConfig("d", 0.5f)));
        plugin.params.processEvents(nullptr, nullptr);
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("d", 0.5f)));
    }

    SECTION("an entry that never held a slot is reusable at once") {
        plugin.params.deactivateDynamicParam("b");
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("d", 0.5f)));
        REQUIRE_FALSE(plugin.params.activateDynamicParam(makeConfig("e", 0.5f)));
    }

    SECTION("a deactivated plugin reuses entries at once") {
        plugin.clapPlugin()->deactivate(plugin.clapPlugin());
        REQUIRE(plugin.params.activateDynamicParam(makeConfig("d", 0.5f)));
    }
}

TEST_CASE("Slot bindings are saved and restored to the same slot", "[params][slots][json]") {
    SlotPlugin plugin;
    REQUIRE(plugin.params.activateDynamicParam(makeConfig("a", 0.5f)));
    REQUIRE(plugin.params.activateDynamicParam(makeConfig("b", 0.5f)));
    REQUIRE(plugin.params.getInfo("a").setAutomatable(true));
    REQUIRE(plugin.params.getInfo("b").setAutomatable(true));
    REQUIRE(plugin.params.getInfo("a").setAutomatable(false));

    applause::json state;
    REQUIRE(plugin.params.saveToJson(state));

    SlotPlugin other;
    REQUIRE(other.params.activateDynamicParam(makeConfig("b", 0.5f)));
    REQUIRE(other.params.activateDynamicParam(makeConfig("a", 0.5f)));
    REQUIRE(other.params.getInfo("a").setAutomatable(true));
    REQUIRE(other.params.loadFromJson(state));

    REQUIRE(other.params.getInfo("b").isAutomatable());
    REQUIRE_FALSE(other.params.getInfo("a").isAutomatable());
    clap_param_info_t slot{};
    REQUIRE(clapParams(other)->get_info(other.clapPlugin(), 1, &slot));
    REQUIRE(std::string(slot.name) == "b");
    REQUIRE(clapParams(other)->get_info(other.clapPlugin(), 0, &slot));
    REQUIRE(std::string(slot.name) == "Unused 1");
}

TEST_CASE("Reusing a dynamic entry never leaks the old range into the new parameter", "[params][slots][threading]") {
    SlotPlugin plugin(nullptr, 1, 1);
    REQUIRE(plugin.clapPlugin()->activate(plugin.clapPlugin(), 48000.0, 32, 512));
    clap_param_info_t slot{};
    REQUIRE(clapParams(plugin)->get_info(plugin.clapPlugin(), 0, &slot));

    std::atomic<bool> stop{false};
    std::thread audio([&] {
        EventList list;
        list.events.push_back(makeEvent(slot.id, slot.cookie, 0.0));
        while (!stop.load(std::memory_order_relaxed)) {
            list.events[0].value = list.events[0].value >= 1.0 ? 0.0 : list.events[0].value + 0.013;
            plugin.params.processEvents(&list.in, nullptr);
        }
    });

    bool leaked = false;
    for (int i = 0; i < 300; ++i) {
        const auto config =
            i % 2 == 0 ? rangedConfig("wide", 20.0f, 20000.0f, 20.0f) : rangedConfig("narrow", 0.0f, 1.0f, 0.0f);
        while (!plugin.params.activateDynamicParam(config)) std::this_thread::yield();
        const auto& info = plugin.params.getInfo(config.string_id);
        info.setAutomatable(true);
        for (int check = 0; check < 20; ++check) {
            const float value = info.getValue();
            if (value < config.min_value || value > config.max_value) leaked = true;
            std::this_thread::yield();
        }
        plugin.params.deactivateDynamicParam(config.string_id);
    }

    stop = true;
    audio.join();
    REQUIRE_FALSE(leaked);
}

TEST_CASE("UI edits queue host messages only for parameters the DAW can see", "[params][slots][ui]") {
    FakeHost fake;
    SlotPlugin plugin(&fake.host);
    auto tab = makeConfig("tab", 0.0f);
    tab.is_internal = true;
    plugin.params.registerParam(tab);
    REQUIRE(plugin.clapPlugin()->init(plugin.clapPlugin()));
    REQUIRE(plugin.params.activateDynamicParam(rangedConfig("rate", 0.0f, 10.0f, 1.0f)));
    const auto& rate = plugin.params.getInfo("rate");

    ParamMessageQueue queue;
    plugin.params.setMessageQueue(&queue);
    float seen = -1.0f;
    rate.on_value_changed.connect([&](float value) { seen = value; });
    ParamMessageQueue::Message message{};

    rate.beginGesture();
    rate.setValueNotifyingHost(5.0f);
    rate.endGesture();
    plugin.params.getInfo("tab").setValueNotifyingHost(1.0f);
    REQUIRE(rate.getValue() == 5.0f);
    REQUIRE(seen == 5.0f);
    REQUIRE_FALSE(queue.toAudio().try_dequeue(message));
    REQUIRE(fake.flush_count == 0);

    REQUIRE(rate.setAutomatable(true));
    rate.setValueNotifyingHost(6.0f);
    REQUIRE(queue.toAudio().try_dequeue(message));
    REQUIRE(message.index == 0);
    REQUIRE(message.value == 6.0f);
    REQUIRE(fake.flush_count == 1);
}

TEST_CASE("Deactivation handlers may activate other dynamic parameters", "[params][dynamic]") {
    DynamicPlugin plugin;
    REQUIRE(plugin.params.activateDynamicParam(makeConfig("a", 0.5f)));
    plugin.params.on_dynamic_param_changed.connect([&](uint32_t, bool active) {
        if (!active) {
            REQUIRE(plugin.params.activateDynamicParam(makeConfig("b", 0.25f)));
            REQUIRE(plugin.params.activateDynamicParam(makeConfig("c", 0.75f)));
        }
    });

    plugin.params.deactivateDynamicParam("a");
    REQUIRE(plugin.params.getInfo("b").getValue() == 0.25f);
    REQUIRE(plugin.params.getInfo("c").getValue() == 0.75f);
    REQUIRE(plugin.params.activateDynamicParam(makeConfig("a", 0.5f)));
}
