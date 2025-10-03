#pragma once

#include <clap/ext/state.h>
#include <clap/stream.h>

#include <functional>
#include <nlohmann/json.hpp>
#include <vector>

#include "applause/core/Extension.h"

namespace applause {
/**
 * @brief Simple JSON transport for CLAP state extension.
 *
 * Converts between CLAP streams and JSON. Nothing more.
 */
class StateExtension : public IExtension {
public:
    using SaveCallback = std::function<bool(nlohmann::json& json)>;
    using LoadCallback = std::function<bool(const nlohmann::json& json)>;

private:
    mutable clap_plugin_state_t clap_struct_{};
    SaveCallback save_callback_;
    LoadCallback load_callback_;

    static bool clap_state_save(const clap_plugin_t* plugin,
                                const clap_ostream_t* stream) noexcept;
    static bool clap_state_load(const clap_plugin_t* plugin,
                                const clap_istream_t* stream) noexcept;

public:
    static constexpr const char* ID = CLAP_EXT_STATE;

    StateExtension();

    const char* id() const override { return ID; }
    const void* getClapExtensionStruct() const override {
        return &clap_struct_;
    }

    void setSaveCallback(SaveCallback callback) { save_callback_ = callback; }
    void setLoadCallback(LoadCallback callback) { load_callback_ = callback; }

    bool isConfigured() const { return save_callback_ && load_callback_; }
};

}  // namespace applause