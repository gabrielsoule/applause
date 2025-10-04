#pragma once

namespace applause {

/**
 * @brief Base interface for all CLAP plugin extensions in the applause
 * framework.
 *
 * This interface provides the foundation for the extension-based architecture,
 * allowing modular plugin features (parameters, GUI, state, audio ports, etc.)
 * to be composed without dealing with C function pointers or manual wiring.
 *
 * Any collection of code that implements one of CLAP's built-in extensions
 * should implement this interface.
 *
 * One awkward requirement is that applause::Extensions MUST declare a
 *
 * "static constexpr const char* ID"
 *
 * which MUST be set to the corresponding CLAP extension ID that the
 * applause::Extension intends to implement.
 *
 * @see PluginBase::registerExtension()
 * @see PluginBase::findExtension()
 */
class IExtension {
public:
    /**
     * @brief Virtual destructor for proper cleanup of derived extension
     * classes.
     */
    virtual ~IExtension() = default;

    /**
     * @brief Returns the CLAP extension identifier string.
     *
     * This must return a valid CLAP extension ID such as:
     * - CLAP_EXT_PARAMS ("clap.params/1")
     * - CLAP_EXT_GUI ("clap.gui/1")
     * - CLAP_EXT_STATE ("clap.state/1")
     * - CLAP_EXT_AUDIO_PORTS ("clap.audio-ports/1")
     *
     * The returned pointer must remain valid for the lifetime of the extension.
     *
     * @return Null-terminated string containing the CLAP extension ID
     */
    virtual const char* id() const = 0;

    /**
     * @brief Returns a pointer to the CLAP C struct containing function
     * pointers.
     *
     * This method provides the C interface that CLAP hosts expect. The returned
     * pointer should be to a struct like clap_plugin_params_t,
     * clap_plugin_gui_t, etc., populated with static C function pointers that
     * dispatch back to this extension instance.
     *
     * @return Pointer to the CLAP extension C struct, or nullptr on error
     */
    virtual const void* getClapExtensionStruct() const = 0;
};

template <typename CExt>
struct CExtensionWrapper : IExtension {
    const char* _ext_id{};
    CExt _ext{};

    constexpr CExtensionWrapper(const char* ext_id, CExt&& ext)
        : _ext_id{ext_id}, _ext{ext} {}

    const char* id() const override { return _ext_id; }

    const void* getClapExtensionStruct() const override { return &_ext; }
};
}  // namespace applause
