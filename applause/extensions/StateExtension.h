#pragma once

#include <clap/ext/state.h>
#include <clap/stream.h>

#include <functional>
#include <vector>

#include "applause/core/Extension.h"
#include "applause/util/Json.h"

namespace applause {
/**
 * The StateExtension provides a mechanism for saving and loading plugin state. You'll probably want to use this.
 *
 * The StateExtension uses JSON to serialize and deserialize state.
 *
 * To use: register the extension during plugin construction, call setSaveCallback() and setLoadCallback() with
 * lambdas that read/write your state into the provided JSON, and return true on success so the host commits the data.
 */
class StateExtension : public IExtension {
public:
    using SaveCallback = std::function<bool(applause::json& json)>;
    using LoadCallback = std::function<bool(const applause::json& json)>;

private:
    mutable clap_plugin_state_t clap_struct_{};
    SaveCallback save_callback_;
    LoadCallback load_callback_;

    static bool clap_state_save(const clap_plugin_t* plugin, const clap_ostream_t* stream) noexcept;
    static bool clap_state_load(const clap_plugin_t* plugin, const clap_istream_t* stream) noexcept;

public:
    static constexpr const char* ID = CLAP_EXT_STATE;

    StateExtension();

    const char* id() const override { return ID; }

    const void* getClapExtensionStruct() const override { return &clap_struct_; }

    /**
     * @brief Sets the handler that serializes plugin state into JSON.
     * @param callback Function that writes state to the given JSON document and returns true on success.
     */
    void setSaveCallback(SaveCallback callback) { save_callback_ = callback; }

    /**
     * @brief Sets the handler that restores plugin state from JSON provided by the host.
     * @param callback Function that reads state from the JSON document and returns true on success.
     */
    void setLoadCallback(LoadCallback callback) { load_callback_ = callback; }

    bool isConfigured() const { return save_callback_ && load_callback_; }
};
}  // namespace applause
