#include "ApplauseExamplePlugin.h"
#include <clap/clap.h>
#include <cstring>
#include "util/DebugHelpers.h"

// Plugin features
static const char* features[] = {
    CLAP_PLUGIN_FEATURE_INSTRUMENT,
    CLAP_PLUGIN_FEATURE_SYNTHESIZER,
    nullptr
};

// Plugin descriptor
static const clap_plugin_descriptor_t descriptor = {
    .clap_version = CLAP_VERSION,
    .id = "com.applause.example",
    .name = "Applause Example",
    .vendor = "Applause Framework",
    .url = "https://github.com/example/applause",
    .manual_url = nullptr,
    .support_url = nullptr,
    .version = "0.1.0",
    .description = "Example plugin using the Applause framework",
    .features = features
};

// Plugin factory implementation
static const clap_plugin_t* factory_create_plugin(
    const clap_plugin_factory_t* factory,
    const clap_host_t* host,
    const char* plugin_id) {
    
    if (!plugin_id || std::strcmp(plugin_id, descriptor.id) != 0) {
        return nullptr;
    }
    
    auto* plugin = new ApplauseExamplePlugin(&descriptor, host);
    return plugin->clapPlugin();
}

static uint32_t factory_get_plugin_count(const clap_plugin_factory_t* factory) {
    return 1;
}

static const clap_plugin_descriptor_t* factory_get_plugin_descriptor(
    const clap_plugin_factory_t* factory,
    uint32_t index) {
    
    if (index == 0) {
        return &descriptor;
    }
    return nullptr;
}

static const clap_plugin_factory_t applause_example_factory = {
    .get_plugin_count = factory_get_plugin_count,
    .get_plugin_descriptor = factory_get_plugin_descriptor,
    .create_plugin = factory_create_plugin
};

// Plugin entry point implementation
static bool clap_init(const char* path) {
    return true;
}

static void clap_deinit() {
}

static const void* clap_get_factory(const char* factory_id) {
    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &applause_example_factory;
    }
    return nullptr;
}

// Export the plugin entry point
extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION,
    .init = clap_init,
    .deinit = clap_deinit,
    .get_factory = clap_get_factory
};