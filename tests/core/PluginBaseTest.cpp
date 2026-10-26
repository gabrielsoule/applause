#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include <applause/core/PluginBase.h>

using namespace applause;

namespace {
const clap_plugin_descriptor_t kDesc{};

struct RecordingExtension : IExtension {
    std::vector<std::string>& log;
    explicit RecordingExtension(std::vector<std::string>& log) : log(log) {}
    const char* id() const override { return "applause.test.recording"; }
    const void* getClapExtensionStruct() const override { return nullptr; }
    void onActivate() noexcept override { log.push_back("extension activate"); }
    void onDeactivate() noexcept override { log.push_back("extension deactivate"); }
    void onMainThread() noexcept override { log.push_back("extension main thread"); }
};

struct RecordingPlugin : PluginBase {
    std::vector<std::string> log;
    RecordingExtension extension{log};
    bool activate_result = true;

    RecordingPlugin() : PluginBase(&kDesc, nullptr) { registerExtension(extension); }
    bool activate(const ProcessInfo&) override {
        log.push_back("plugin activate");
        return activate_result;
    }
    void deactivate() override { log.push_back("plugin deactivate"); }
    void onMainThread() override { log.push_back("plugin main thread"); }
    ProcessStatus process(ProcessContext&) noexcept override { return ProcessStatus::Continue; }
};
}  // namespace

TEST_CASE("PluginBase forwards lifecycle and main-thread callbacks to extensions", "[plugin]") {
    RecordingPlugin plugin;
    auto* clap = plugin.clapPlugin();

    REQUIRE(clap->activate(clap, 48000.0, 32, 512));
    clap->on_main_thread(clap);
    clap->deactivate(clap);

    REQUIRE(plugin.log == std::vector<std::string>{"plugin activate", "extension activate", "extension main thread",
                                                   "plugin main thread", "extension deactivate",
                                                   "plugin deactivate"});
}

TEST_CASE("PluginBase skips extension activation when the plugin fails to activate", "[plugin]") {
    RecordingPlugin plugin;
    plugin.activate_result = false;
    auto* clap = plugin.clapPlugin();

    REQUIRE_FALSE(clap->activate(clap, 48000.0, 32, 512));
    REQUIRE(plugin.log == std::vector<std::string>{"plugin activate"});
}
