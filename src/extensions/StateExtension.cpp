#include "StateExtension.h"
#include "core/PluginBase.h"

namespace applause
{
    bool StateExtension::clap_state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) noexcept
    {
        auto* ext = PluginBase::findExtension<StateExtension>(plugin);
        if (!ext || !ext->save_impl_) return false;

        return ext->save_impl_(stream);
    }

    bool StateExtension::clap_state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) noexcept
    {
        auto* ext = PluginBase::findExtension<StateExtension>(plugin);
        if (!ext || !ext->load_impl_) return false;

        return ext->load_impl_(stream);
    }

    StateExtension::StateExtension()
    {
        clap_struct_ = {};
        clap_struct_.save = clap_state_save;
        clap_struct_.load = clap_state_load;
    }
} // namespace applause
