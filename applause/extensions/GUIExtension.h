#pragma once

#include <clap/ext/gui.h>

#include "applause/core/Extension.h"
#ifdef __linux__
#include <clap/ext/posix-fd-support.h>
#endif
#include <functional>
#include <memory>
#include <string>

#include "applause/ui/IEditor.h"

namespace applause {

/**
 * @class GUIExtension
 * @brief CLAP GUI extension that manages editor lifecycle and host
 * communication.
 *
 * This extension provides a framework-agnostic way to integrate GUI editors
 * with CLAP hosts.
 *
 * The extension doesn't actually implement a GUI; rather, it is a C++ glue
 * layer that lets generic UI editors communicate with the host application and
 * respond to parameter changes.
 *
 * To implement your own GUI system, override and implement IEditor.h. Of
 * course, we recommend the built-in GPU-accelerated ApplauseEditor, which makes
 * building a GUI with parameter integration quick and easy.
 */
class GUIExtension : public IExtension {
public:
    /**
     * @brief Factory function type for creating editor instances.
     */
    using EditorFactory = std::function<std::unique_ptr<IEditor>()>;

    /**
     * @brief Construct a GUI extension using your plugin's supplied editor
     * factory lambda.
     *
     * @param host The CLAP host
     * @param factory Function that creates editor instances
     * @param defaultWidth Default editor width in pixels. It is recommended to
     * use a height and width that correspond to a nice integer aspect ratio
     * (e.g. 16:9, 4:3, etc)
     * @param defaultHeight Default editor height in pixels. It is recommended
     * to use a height and width that correspond to a nice integer aspect ratio
     * (e.g. 16:9, 4:3, etc)
     * @param fixedAspectRatio Whether to maintain aspect ratio on resize.
     * //TODO Add auto UI scaling when this is true
     */
    explicit GUIExtension(const clap_host_t* host, EditorFactory factory,
                          uint32_t defaultWidth = 800,
                          uint32_t defaultHeight = 600,
                          bool fixedAspectRatio = false);

    static constexpr const char* ID = CLAP_EXT_GUI;
    const char* id() const override { return ID; }
    const void* getClapExtensionStruct() const override {
        return &clap_struct_;
    }

    /**
     * @brief Get the current editor instance.
     * @return Pointer to the editor or nullptr if not created
     */
    IEditor* getEditor() { return editor_.get(); }

    // Host GUI callback helpers
    bool requestResize(uint32_t width, uint32_t height);
    bool requestShow();
    bool requestHide();

private:
    // Static CLAP callbacks
    static bool clap_gui_is_api_supported(const clap_plugin_t* plugin,
                                          const char* api,
                                          bool is_floating) noexcept;

    static bool clap_gui_get_preferred_api(const clap_plugin_t* plugin,
                                           const char** api,
                                           bool* is_floating) noexcept;

    static bool clap_gui_create(const clap_plugin_t* plugin, const char* api,
                                bool is_floating) noexcept;

    static void clap_gui_destroy(const clap_plugin_t* plugin) noexcept;

    static bool clap_gui_set_scale(const clap_plugin_t* plugin,
                                   double scale) noexcept;

    static bool clap_gui_get_size(const clap_plugin_t* plugin, uint32_t* width,
                                  uint32_t* height) noexcept;

    static bool clap_gui_can_resize(const clap_plugin_t* plugin) noexcept;

    static bool clap_gui_get_resize_hints(
        const clap_plugin_t* plugin, clap_gui_resize_hints_t* hints) noexcept;

    static bool clap_gui_adjust_size(const clap_plugin_t* plugin,
                                     uint32_t* width,
                                     uint32_t* height) noexcept;

    static bool clap_gui_set_size(const clap_plugin_t* plugin, uint32_t width,
                                  uint32_t height) noexcept;

    static bool clap_gui_set_parent(const clap_plugin_t* plugin,
                                    const clap_window_t* window) noexcept;

    static bool clap_gui_set_transient(const clap_plugin_t* plugin,
                                       const clap_window_t* window) noexcept;

    static void clap_gui_suggest_title(const clap_plugin_t* plugin,
                                       const char* title) noexcept;

    static bool clap_gui_show(const clap_plugin_t* plugin) noexcept;

    static bool clap_gui_hide(const clap_plugin_t* plugin) noexcept;

#ifdef __linux__
    // Linux POSIX FD support
    static void clap_on_posix_fd(const clap_plugin_t* plugin, int fd,
                                 clap_posix_fd_flags_t flags) noexcept;
#endif

    // CLAP GUI extension struct
    static constexpr clap_plugin_gui_t clap_struct_ = {
        .is_api_supported = clap_gui_is_api_supported,
        .get_preferred_api = clap_gui_get_preferred_api,
        .create = clap_gui_create,
        .destroy = clap_gui_destroy,
        .set_scale = clap_gui_set_scale,
        .get_size = clap_gui_get_size,
        .can_resize = clap_gui_can_resize,
        .get_resize_hints = clap_gui_get_resize_hints,
        .adjust_size = clap_gui_adjust_size,
        .set_size = clap_gui_set_size,
        .set_parent = clap_gui_set_parent,
        .set_transient = clap_gui_set_transient,
        .suggest_title = clap_gui_suggest_title,
        .show = clap_gui_show,
        .hide = clap_gui_hide};

    // Member variables
    const clap_host_t* host_;
    const clap_host_gui_t* hostGui_;
#ifdef __linux__
    const clap_host_posix_fd_support_t* hostFdSupport_;
#endif
    EditorFactory editor_factory_;
    std::unique_ptr<IEditor> editor_;
    uint32_t width_;
    uint32_t height_;
    bool fixedAspectRatio_;
    float aspectRatio_;

    // Internal methods
    void createEditor();
    void destroyEditor();

#ifdef __linux__
    // Called when Linux FD events occur
    void onPosixFd(int fd, clap_posix_fd_flags_t flags) noexcept;
#endif
};

}  // namespace applause