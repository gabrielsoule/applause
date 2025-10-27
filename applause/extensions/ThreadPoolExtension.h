#pragma once

#include <applause/core/Extension.h>
#include <clap/ext/thread-pool.h>

#include <functional>

namespace applause {

/**
 * @brief Bridges the CLAP thread pool extension to plugin code.
 *
 * ThreadPoolExtension exposes the host-provided `clap_host_thread_pool_t`
 * interface and allows plugins to run work on the host's background threads.
 */
class ThreadPoolExtension : public IExtension {
public:
    static constexpr const char* ID = CLAP_EXT_THREAD_POOL;

    ThreadPoolExtension() = default;

    void onHostReady() noexcept override;

    /**
     * @brief Checks whether the host exposes the CLAP thread pool extension.
     * @return True when a valid `clap_host_thread_pool_t` is available.
     */
    bool hasHostSupport() const noexcept { return host_pool_ && host_pool_->request_exec; }

    const char* id() const override { return ID; }

    const void* getClapExtensionStruct() const override { return &clap_struct_; }

    /**
     * @brief Registers a callback invoked for each scheduled task.
     * @param callback Functor that receives the task index supplied by the host.
     */
    void setCallback(std::function<void(uint32_t)> callback) { callback_ = std::move(callback); }

    /**
     * @brief Executes the registered callback for the given task.
     * @param task_index Task identifier provided by the host scheduler.
     */
    void exec(uint32_t task_index) const {
        if (callback_) {
            callback_(task_index);
        }
    }

    [[nodiscard]] bool requestExec(uint32_t num_tasks) const noexcept;

private:
    static void clap_exec(const clap_plugin_t* plugin, uint32_t task_index) noexcept;

    static constexpr clap_plugin_thread_pool_t clap_struct_ = {.exec = clap_exec};

    const clap_host_thread_pool_t* host_pool_ = nullptr;
    std::function<void(uint32_t)> callback_;
};
}  // namespace applause
