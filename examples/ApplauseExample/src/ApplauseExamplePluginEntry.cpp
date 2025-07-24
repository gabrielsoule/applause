#include "ApplauseExamplePlugin.h"
#include <clap/clap.h>
#include <cstring>

// Plugin entry point implementation
static bool clap_init(const char* path) {
    return true;
}

static void clap_deinit() {
}

static const void* clap_get_factory(const char* factory_id) {
    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &ApplauseExamplePlugin::factory;
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