#pragma once

#include <applause/core/Extension.h>
#include <clap/ext/thread-pool.h>

#include <functional>

namespace applause {
// Thin wrapper around the CLAP thread-pool extension. Bridges the plugin-side
// exec callback and caches the host-side request API.
class ThreadPoolExtension : public IExtension {
public:
    static constexpr const char* ID = CLAP_EXT_THREAD_POOL;

    ThreadPoolExtension() = default;

    void onHostReady() noexcept override;

    // [thread-safe] Host support is optional; this reports availability.
    bool hasHostSupport() const noexcept { return host_pool_ && host_pool_->request_exec; }

    const char* id() const override { return ID; }

    const void* getClapExtensionStruct() const override { return &clap_struct_; }

    // Install the per-task callback. [main-thread]
    void setCallback(std::function<void(uint32_t)> callback) { callback_ = std::move(callback); }

    // Called by the host's worker threads. [audio-thread or host pool]
    void exec(uint32_t task_index) const {
        if (callback_) {
            callback_(task_index);
        }
    }

    // Request execution on the host pool. Blocks until tasks finish.
    // Returns false if the host rejects. [audio-thread]
    [[nodiscard]] bool requestExec(uint32_t num_tasks) const noexcept;

private:
    // CLAP C callback - bridges to instance method
    static void clap_exec(const clap_plugin_t* plugin, uint32_t task_index) noexcept;

    // CLAP struct
    static constexpr clap_plugin_thread_pool_t clap_struct_ = {.exec = clap_exec};

    const clap_host_thread_pool_t* host_pool_ = nullptr;
    std::function<void(uint32_t)> callback_;
};
}  // namespace applause
