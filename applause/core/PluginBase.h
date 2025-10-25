#pragma once

#include <applause/util/MemoryArena.h>
#include <applause/core/ProcessInfo.h>
#include <clap/clap.h>

#include <string>
#include <unordered_map>

#include "Extension.h"

namespace applause {

/**
 * @class PluginBase
 * @brief Base class for the implementation of an Applause plugin.
 *
 * This class handles the core lifecycle and processing callbacks, as well
 * as the registration and retrieval of extensions. Implementers should
 * inherit from PluginBase to customize plugin behavior by overriding the
 * virtual methods.
 */
class PluginBase {
private:
    clap_plugin_t _plugin;
    const clap_host_t* _host;
    std::unordered_map<std::string, IExtension*> _extensions;
    bool extensions_connected_ = false;

    // Static C function dispatchers for core plugin functions
    static bool clapInit(const clap_plugin_t* plugin) noexcept {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        bool ok = self->init();
        if (ok) {
            for (auto& [id, ext] : self->_extensions) {
                (void)id;
                ext->assignHost(self->_host);
            }
        }
        return ok;
    }

    static void clapDestroy(const clap_plugin_t* plugin) noexcept {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        self->destroy();
        delete self;
    }

    static bool clapActivate(const clap_plugin_t* plugin, double sample_rate,
                             uint32_t min_frames_count,
                             uint32_t max_frames_count) noexcept {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        const ProcessInfo info{
            .sample_rate = sample_rate,
            .min_frame_size = min_frames_count,
            .max_frame_size = max_frames_count
        };
        return self->activate(info);
    }

    static void clapDeactivate(const clap_plugin_t* plugin) noexcept {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        self->deactivate();
    }

    static bool clapStartProcessing(const clap_plugin_t* plugin) noexcept {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        return self->startProcessing();
    }

    static void clapStopProcessing(const clap_plugin_t* plugin) noexcept {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        self->stopProcessing();
    }

    static void clapReset(const clap_plugin_t* plugin) noexcept {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        self->reset();
    }

    static clap_process_status clapProcess(
        const clap_plugin_t* plugin, const clap_process_t* process) noexcept {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        return self->process(process);
    }

    static const void* clapGetExtension(const clap_plugin_t* plugin,
                                        const char* id) noexcept {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        auto it = self->_extensions.find(id);
        return it != self->_extensions.end()
                   ? it->second->getClapExtensionStruct()
                   : nullptr;
    }

    static void clapOnMainThread(const clap_plugin_t* plugin) noexcept {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        self->onMainThread();
    }

protected:
    PluginBase(const clap_plugin_descriptor_t* desc, const clap_host_t* host)
        : _host(host) {
        // Initialize the C struct with our static dispatchers
        _plugin = {};
        _plugin.desc = desc;
        _plugin.plugin_data = this;
        _plugin.init = clapInit;
        _plugin.destroy = clapDestroy;
        _plugin.activate = clapActivate;
        _plugin.deactivate = clapDeactivate;
        _plugin.start_processing = clapStartProcessing;
        _plugin.stop_processing = clapStopProcessing;
        _plugin.reset = clapReset;
        _plugin.process = clapProcess;
        _plugin.get_extension = clapGetExtension;
        _plugin.on_main_thread = clapOnMainThread;
    }

    virtual ~PluginBase() = default;

    void registerExtension(IExtension& ext) {
        _extensions[ext.id()] = &ext;
        if (extensions_connected_) {
            ext.assignHost(_host);
        }
    }

    /**
     * @brief Find an extension by type
     *
     * This is a convenience wrapper around the static findExtension().
     *
     * If you need to access an extension regularly within audio processing code,
     * e.g. every block, it is more efficient to call this once during
     * preparation and then use the cached pointer during rendering.
     *
     * @tparam ExtType The extension type
     * @return Pointer to extension if registered, nullptr otherwise
     *
     * @code
     * auto* ports = getExtension<AudioPortsExtension>();
     * if (ports) {
     *     // Extension is present! Go wild!
     * }
     * @endcode
     */
    template <typename ExtType>
    ExtType* getExtension() {
        auto it = _extensions.find(ExtType::ID);
        return it != _extensions.end() ? static_cast<ExtType*>(it->second)
                                       : nullptr;
    }

    /**
     * @brief Find an extension by type (const version).
     */
    template <typename ExtType>
    const ExtType* getExtension() const {
        auto it = _extensions.find(ExtType::ID);
        return it != _extensions.end()
                   ? static_cast<const ExtType*>(it->second)
                   : nullptr;
    }

    // Access to host
    const clap_host_t* host() const { return _host; }

public:
    // Helper for extensions to find themselves from C callbacks
    template <typename ExtType>
    static ExtType* findExtension(const clap_plugin_t* plugin) {
        auto* self = static_cast<PluginBase*>(plugin->plugin_data);
        auto it = self->_extensions.find(ExtType::ID);
        if (it != self->_extensions.end()) {
            return static_cast<ExtType*>(it->second);
        }
        return nullptr;
    }

    // Get the C plugin struct
    clap_plugin_t* clapPlugin() { return &_plugin; }

    // Virtual methods for plugin implementation
    virtual bool init() { return true; }

    virtual void destroy() {}

    /**
     * @brief Activate the plugin for audio processing.
     *
     * Called by the host to prepare the plugin for audio processing.
     * The plugin should allocate resources and prepare buffers based
     * on the provided configuration.
     *
     * @param info Processing configuration from the host
     * @return true if activation succeeded, false otherwise
     */
    virtual bool activate(const ProcessInfo& info) {
        return true;
    }

    virtual void deactivate() {}

    virtual bool startProcessing() { return true; }

    virtual void stopProcessing() {}

    virtual void reset() {}

    virtual clap_process_status process(const clap_process_t* process) = 0;

    virtual void onMainThread() {}
};
}  // namespace applause
