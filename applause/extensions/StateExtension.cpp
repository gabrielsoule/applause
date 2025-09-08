#include "StateExtension.h"
#include "applause/core/PluginBase.h"
#include "applause/util/DebugHelpers.h"

namespace applause
{
    StateExtension::StateExtension()
    {
        clap_struct_ = {};
        clap_struct_.save = clap_state_save;
        clap_struct_.load = clap_state_load;
    }

    bool StateExtension::clap_state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) noexcept
    {
        auto* ext = PluginBase::findExtension<StateExtension>(plugin);
        if (!ext || !ext->save_callback_) return false;

        try {
            nlohmann::json state;
            
            // Let plugin populate the state
            if (!ext->save_callback_(state)) {
                return false;
            }
            
            // Convert to string
            std::string json_str = state.dump();
            
            // Write to stream with chunking
            const char* ptr = json_str.data();
            size_t remaining = json_str.size();
            
            while (remaining > 0) {
                int64_t written = stream->write(stream, ptr, remaining);
                if (written <= 0) return false;
                ptr += written;
                remaining -= written;
            }
            
            return true;
            
        } catch (...) {
            return false;
        }
    }

    bool StateExtension::clap_state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) noexcept
    {
        auto* ext = PluginBase::findExtension<StateExtension>(plugin);
        if (!ext || !ext->load_callback_) return false;

        try {
            // Read entire stream
            std::vector<char> buffer;
            constexpr size_t CHUNK_SIZE = 4096;
            char chunk[CHUNK_SIZE];
            
            while (true) {
                int64_t bytes_read = stream->read(stream, chunk, CHUNK_SIZE);
                if (bytes_read < 0) return false;
                if (bytes_read == 0) break;
                buffer.insert(buffer.end(), chunk, chunk + bytes_read);
            }
            
            // Parse JSON
            auto state = nlohmann::json::parse(buffer.begin(), buffer.end());
            
            // Let plugin load the state
            return ext->load_callback_(state);
            
        } catch (...) {
            return false;
        }
    }

} // namespace applause