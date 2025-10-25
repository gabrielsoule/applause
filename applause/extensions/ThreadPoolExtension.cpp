#include "ThreadPoolExtension.h"

#include "applause/core/PluginBase.h"

namespace applause {
void ThreadPoolExtension::onHostReady() noexcept {
    host_pool_ = nullptr;
    if (host_) {
        host_pool_ = static_cast<const clap_host_thread_pool_t*>(host_->get_extension(host_, CLAP_EXT_THREAD_POOL));
    }
}

bool ThreadPoolExtension::requestExec(uint32_t num_tasks) const noexcept {
    if (!hasHostSupport()) {
        return false;
    }
    return host_pool_->request_exec(host_, num_tasks);
}

void ThreadPoolExtension::clap_exec(const clap_plugin_t* plugin, uint32_t task_index) noexcept {
    auto* ext = PluginBase::findExtension<ThreadPoolExtension>(plugin);
    if (ext) {
        ext->exec(task_index);
    }
}
}  // namespace applause
